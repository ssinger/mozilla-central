/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nspr.h"
#include "nsIURL.h"
#include "nsHTTPRequest.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsHTTPAtoms.h"
#include "nsHTTPEnums.h"
#include "nsIPipe.h"
#include "nsIStringStream.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsHTTPChannel.h"
#include "nsHTTPResponseListener.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"
#include "nsIIOService.h"
#include "nsAuthEngine.h"
#include "nsIServiceManager.h"
#include "nsISocketTransport.h"
#include "plstr.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);
static NS_DEFINE_CID(kHTTPHandlerCID,   NS_IHTTPHANDLER_CID);

extern nsresult DupString(char* *o_Dest, const char* i_Src);

nsHTTPRequest::nsHTTPRequest(nsIURI* i_URL, nsHTTPHandler* i_Handler, PRUint32 bufferSegmentSize, PRUint32 bufferMaxSize, HTTPMethod i_Method)
    :
    mMethod(i_Method),
    mVersion(HTTP_ONE_ZERO),
    mCapabilities (0),
    mKeepAliveTimeout (0),
    mAttempts (0),
    mRequestSpec(0),
    mHandler (i_Handler),
    mBufferSegmentSize(bufferSegmentSize),
    mBufferMaxSize(bufferMaxSize),
    mAbortStatus(NS_OK)
{   
    NS_INIT_REFCNT();

    NS_ASSERTION(i_URL, "No URL for the request!!");
    mURI = do_QueryInterface(i_URL);

#if defined(PR_LOGGING)
  nsXPIDLCString urlCString; 
  mURI->GetSpec(getter_Copies(urlCString));
  
  PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
         ("Creating nsHTTPRequest [this=%x] for URI: %s.\n", 
           this, (const char *)urlCString));
#endif
  
    mHandler -> GetHttpVersion      (&mVersion);
    mHandler -> GetKeepAliveTimeout (&mKeepAliveTimeout);
}
    

nsHTTPRequest::~nsHTTPRequest()
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Deleting nsHTTPRequest [this=%x].\n", this));

    mTransport = 0;
    CRTFREEIF(mRequestSpec);
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

NS_IMPL_THREADSAFE_ISUPPORTS2(nsHTTPRequest, nsIStreamObserver, nsIRequest)

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsHTTPRequest::IsPending(PRBool *result)
{
  nsresult rv = NS_OK;

  if (mTransport) {
    rv = mTransport->IsPending(result);
  } else {
    *result = PR_FALSE; 
  }

  return rv;
}

NS_IMETHODIMP
nsHTTPRequest::GetStatus(nsresult *status)
{
    *status = mAbortStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequest::Cancel(nsresult status)
{
  nsresult rv = NS_ERROR_FAILURE;

  mAbortStatus = status;
  if (mTransport) {
    rv = mTransport->Cancel(status);
  }
  return rv;
}

NS_IMETHODIMP
nsHTTPRequest::Suspend(void)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mTransport) {
    rv = mTransport->Suspend();
  }
  return rv;
}

NS_IMETHODIMP
nsHTTPRequest::Resume(void)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mTransport) {
    rv = mTransport->Resume();
  }
  return rv;
}


// Finally our own methods...

nsresult nsHTTPRequest::WriteRequest ()
{
    nsresult rv;
    if (!mURI) {
        NS_ERROR("No URL to build request for!");
        return NS_ERROR_NULL_POINTER;
    }

    if (mAttempts > 2)
        return NS_ERROR_FAILURE;

    if (!mTransport)
    {
        rv = mHandler -> RequestTransport (mURI, mConnection, mBufferSegmentSize, mBufferMaxSize, 
                                           getter_AddRefs (mTransport),
                                           &mCapabilities,
                                           mAttempts ? TRANSPORT_OPEN_ALWAYS : TRANSPORT_REUSE_ALIVE);

        if (NS_FAILED (rv))
            return rv;
        rv = mTransport -> SetNotificationCallbacks (mConnection);
    }    

    if (NS_FAILED (rv))
        return rv;

    if (mAttempts++ == 0)
        formHeaders ();

    //
    // Build up the request into mRequestBuffer...
    //
    nsXPIDLCString autoBuffer;
    nsAutoString   autoString;

    mRequestBuffer.Truncate();

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPRequest::Build() [this=%x].  Building Request string.\n",
            this));
    //
    // Build the request line...
    //
    // ie. Method SP Request-URI SP HTTP-Version CRLF
    //
    mRequestBuffer.Append (MethodToString (mMethod));

    // Request spec gets set for proxied cases-
    if (!mRequestSpec)
    {
        rv = mURI->GetPath(getter_Copies(autoBuffer));
        mRequestBuffer.Append(autoBuffer);
    }
    else
        mRequestBuffer.Append(mRequestSpec);
    
    //if (mRequestSpec) 
    /*
    else
    {
        if (aIsProxied) {
            rv = mURI->GetSpec(getter_Copies(autoBuffer));
        } else {
            rv = mURI->GetPath(getter_Copies(autoBuffer));
        }
        mRequestBuffer.Append(autoBuffer);
    }
    */
    
    //Trim off the # portion if any...
    int refLocation = mRequestBuffer.RFind("#");
    if (-1 != refLocation)
        mRequestBuffer.Truncate(refLocation);

    char * httpVersion = " HTTP/1.0" CRLF;

    switch (mVersion)
    {
        case HTTP_ZERO_NINE:
            httpVersion = " HTTP/0.9"CRLF;
            break;
        case HTTP_ONE_ONE:
            httpVersion = " HTTP/1.1"CRLF;
    }

    mRequestBuffer.Append (httpVersion);

    //
    // Write the request headers, if any...
    //
    // ie. field-name ":" [field-value] CRLF
    //
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = mHeaders.GetEnumerator (getter_AddRefs (enumerator));

    if (NS_SUCCEEDED(rv)) {
        PRBool bMoreHeaders;
        nsCOMPtr<nsISupports>   item;
        nsCOMPtr<nsIHTTPHeader> header;
        nsCOMPtr<nsIAtom>       headerAtom;

        enumerator->HasMoreElements(&bMoreHeaders);
        while (bMoreHeaders) {
            enumerator->GetNext(getter_AddRefs(item));
            header = do_QueryInterface(item);

            NS_ASSERTION(header, "Bad HTTP header.");
            if (header) {
                header->GetField(getter_AddRefs(headerAtom));
                nsXPIDLCString fieldName;
                header->GetFieldName(getter_Copies(fieldName));
                NS_ASSERTION(fieldName, "field name not returned!, \
                        out of memory?");
                mRequestBuffer.Append(fieldName);
                header->GetValue(getter_Copies(autoBuffer));

                mRequestBuffer.Append(": ");
                mRequestBuffer.Append(autoBuffer);
                mRequestBuffer.Append(CRLF);
            }
            enumerator->HasMoreElements(&bMoreHeaders);
        }
    }

    NS_ASSERTION(mConnection, "Connection disappeared!");

    // Currently nsIPostStreamData contains the header info and the data.
    // So we are forced to putting this here in the end. 
    // This needs to change! since its possible for someone to modify it
    // TODO- Gagan
    if (!mPostDataStream) {
        mRequestBuffer.Append(CRLF);
    }

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("\nnsHTTPRequest::Build() [this=%x].\tWriting Request:\n"
            "=== Start\n%s=== End\n",
            this, mRequestBuffer.GetBuffer()));

    //
    // Create a string stream to feed to the socket transport.  This stream
    // contains only the request.  Any POST data will be sent in a second
    // AsyncWrite to the server..
    //
    nsCOMPtr<nsISupports> result;
    nsCOMPtr<nsIInputStream> stream;

    rv = NS_NewCharInputStream(getter_AddRefs(result), 
                               mRequestBuffer.GetBuffer());
    if (NS_FAILED(rv)) return rv;

    stream = do_QueryInterface(result, &rv);
    if (NS_FAILED(rv)) return rv;

    //
    // Write the request to the server.  
    //
    rv = mTransport->SetTransferCount(mRequestBuffer.Length());
    if (NS_FAILED(rv)) return rv;
    rv = mTransport->AsyncWrite(stream, this, (nsISupports*)(nsIRequest*)mConnection);
    return rv;
}

nsresult nsHTTPRequest::Clone(const nsHTTPRequest* *o_Request) const
{
    return NS_ERROR_FAILURE;
}
                        
nsresult nsHTTPRequest::SetMethod(HTTPMethod i_Method)
{
    mMethod = i_Method;
    return NS_OK;
}

HTTPMethod nsHTTPRequest::GetMethod(void) const
{
    return mMethod;
}
                        
nsresult nsHTTPRequest::SetPriority()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsHTTPRequest::GetPriority()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult nsHTTPRequest::SetHeader(nsIAtom* i_Header, const char* i_Value)
{
    return mHeaders.SetHeader (i_Header, i_Value);
}

nsresult nsHTTPRequest::GetHeader(nsIAtom* i_Header, char* *o_Value)
{
    return mHeaders.GetHeader (i_Header, o_Value);
}


nsresult nsHTTPRequest::SetHTTPVersion (PRUint32  i_Version)
{
    mVersion = i_Version;
    return NS_OK;
}

nsresult nsHTTPRequest::GetHTTPVersion (PRUint32 * o_Version) 
{
    *o_Version = mVersion;
    return NS_OK;
}


nsresult nsHTTPRequest::SetPostDataStream(nsIInputStream* aStream)
{
  mPostDataStream = aStream;

  return NS_OK;
}

nsresult nsHTTPRequest::GetPostDataStream(nsIInputStream* *aResult)
{ 
  nsresult rv = NS_OK;

  if (aResult) {
    *aResult = mPostDataStream;
    NS_IF_ADDREF(*aResult);
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHTTPRequest::OnStartRequest(nsIChannel* channel, nsISupports* i_Context)
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPRequest [this=%x]. Starting to write data to the server.\n",
            this));
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequest::OnStopRequest (nsIChannel* channel, nsISupports* i_Context,
                              nsresult iStatus,
                              const PRUnichar* i_Msg)
{
    nsresult rv;
    nsCOMPtr<nsISocketTransport> trans = do_QueryInterface (mTransport, &rv);
    
    rv = iStatus;

    if (NS_SUCCEEDED (rv))
    {
        PRBool isAlive = PR_TRUE;
        if (trans)
            trans -> IsAlive (0, &isAlive);

        if (isAlive)
        {
            //
            // Write the POST data out to the server...
            //
            if (mPostDataStream)
            {
                NS_ASSERTION(mMethod == HM_POST, "Post data without a POST method?");

                PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
                    ("nsHTTPRequest [this=%x]. Writing POST data to the server.\n", this));

                rv = mTransport -> AsyncWrite (mPostDataStream, this, (nsISupports*)(nsIRequest*)mConnection);

                /* the mPostDataStream is released below... */
            }
            //
            // Prepare to receive the response...
            //
            else
            {
                PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
                       ("nsHTTPRequest [this=%x]. Finished writing request to server." "\tStatus: %x\n", this, iStatus));

                nsHTTPResponseListener* pListener = new nsHTTPServerListener (mConnection, mHandler);
                if (pListener)
                {
                    NS_ADDREF  (pListener);
                    rv = mTransport -> AsyncRead (pListener, i_Context);
                    NS_RELEASE (pListener);
                }
                else
                    rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }
        else
            rv = NS_ERROR_FAILURE; /* isAlive */
    } /* NS_SUCCEEDED */

    //
    // An error occurred when trying to write the request to the server!
    //
    else
    {
        PR_LOG (gHTTPLog, PR_LOG_ERROR, ("nsHTTPRequest [this=%x]. Error writing request to server." "\tStatus: %x\n", this, iStatus));
        rv = iStatus;
    }

    //
    // An error occurred...  Finish the transaction and notify the consumer
    // of the failure...
    //
    if (NS_FAILED (rv))
    {
        PRUint32 wasKeptAlive = 0;

        if (trans)
            trans  -> GetReuseCount (&wasKeptAlive);

        mHandler   -> ReleaseTransport (mTransport, 0);
        mTransport = null_nsCOMPtr ();

        if (wasKeptAlive)
        {
            rv = WriteRequest ();
            
            if (NS_SUCCEEDED (rv))
                return rv;
        }

        // Notify the HTTPChannel that the request has finished
        nsCOMPtr<nsIStreamListener> consumer;

        mConnection -> GetResponseDataListener (getter_AddRefs (consumer));
        mConnection -> ResponseCompleted (consumer, rv, i_Msg);
    }
 
    //
    // These resouces are no longer needed...
    //
    mRequestBuffer.Truncate ();
    mPostDataStream = null_nsCOMPtr ();

    return rv;
}


nsresult nsHTTPRequest::SetConnection(nsHTTPChannel* i_Connection)
{
    mConnection = i_Connection;
    return NS_OK;
}

nsresult nsHTTPRequest::SetTransport (nsIChannel * aTransport)
{
    mTransport = aTransport;
    return NS_OK;
}

nsresult nsHTTPRequest::GetTransport (nsIChannel **aTransport)
{
    *aTransport = mTransport;
    return NS_OK;
}

nsresult nsHTTPRequest::GetHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    return mHeaders.GetEnumerator (aResult);
}

nsresult
nsHTTPRequest::SetOverrideRequestSpec(const char* i_Spec)
{
    CRTFREEIF(mRequestSpec);
    return DupString(&mRequestSpec, i_Spec);
}

nsresult
nsHTTPRequest::GetOverrideRequestSpec(char** o_Spec)
{
    return DupString(o_Spec, mRequestSpec);
}

nsresult
nsHTTPRequest::formHeaders ()
{
    nsXPIDLCString host;
    mURI -> GetHost (getter_Copies (host));
    PRInt32 port = -1;
    mURI -> GetPort (&port);

    // Send Host header by default
    if (HTTP_ZERO_NINE != mVersion)
    {
        if (-1 != port)
        {
            char* tempHostPort = 
                PR_smprintf("%s:%d", (const char*)host, port);
            if (tempHostPort)
            {
                SetHeader(nsHTTPAtoms::Host, tempHostPort);
                PR_smprintf_free (tempHostPort);
                tempHostPort = 0;
            }
        }
        else
            SetHeader(nsHTTPAtoms::Host, host);
    }

    // Add the user-agent 
    nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIHTTPProtocolHandler, httpHandler, kHTTPHandlerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString ua;
    if (NS_SUCCEEDED (httpHandler -> GetUserAgent (getter_Copies(ua))))
    {
        nsCAutoString uaString ((const PRUnichar*)ua);
        SetHeader (nsHTTPAtoms::User_Agent, uaString.GetBuffer ());
    }

    PRUint32 loadAttributes;
    mConnection -> GetLoadAttributes (&loadAttributes);
    
    if (loadAttributes & (nsIChannel::FORCE_VALIDATION | 
                nsIChannel::FORCE_RELOAD))
    {

        // We need to send 'Pragma:no-cache' to inhibit proxy caching even if
        // no proxy is configured since we might be talking with a transparent
        // proxy, i.e. one that operates at the network level.  See bug #14772
        SetHeader (nsHTTPAtoms::Pragma, "no-cache");
    }

    if (loadAttributes & nsIChannel::FORCE_RELOAD)
    {
        // If doing a reload, force end-to-end 
        SetHeader (nsHTTPAtoms::Cache_Control, "no-cache");
    }
    else
    if (loadAttributes & nsIChannel::FORCE_VALIDATION)
    {
        // A "max-age=0" cache-control directive forces each cache along the
        // path to the origin server to revalidate its own entry, if any, with
        // the next cache or server.
        SetHeader (nsHTTPAtoms::Cache_Control, "max-age=0");
    }

    // Send */*. We're no longer chopping MIME-types for acceptance.
    // MIME based content negotiation has died.
    // SetHeader(nsHTTPAtoms::Accept, "image/gif, image/x-xbitmap, image/jpeg, 
    // image/pjpeg, image/png, */*");
    SetHeader (nsHTTPAtoms::Accept, "*/*");

    nsXPIDLCString acceptLanguages;
    // Add the Accept-Language header
    rv = httpHandler->GetAcceptLanguages (getter_Copies(acceptLanguages));
    
    if (NS_SUCCEEDED(rv))
    {
        if (acceptLanguages && *acceptLanguages)
            SetHeader (nsHTTPAtoms::Accept_Language, acceptLanguages);
    }

    nsXPIDLCString acceptEncodings;
    rv = httpHandler -> GetAcceptEncodings (getter_Copies (acceptEncodings));
    
    if (NS_SUCCEEDED(rv))
    {
        if (acceptEncodings && *acceptEncodings)
            SetHeader (nsHTTPAtoms::Accept_Encoding, acceptEncodings);
    }

    if (mCapabilities & (nsIHTTPProtocolHandler::ALLOW_KEEPALIVE|nsIHTTPProtocolHandler::ALLOW_PROXY_KEEPALIVE))
    {
        char *p = PR_smprintf ("%d", mKeepAliveTimeout);
        
        SetHeader (nsHTTPAtoms::Keep_Alive, p);
        PR_smprintf_free (p);

        SetHeader (nsHTTPAtoms::Connection, "keep-alive");
    }
    else
        SetHeader (nsHTTPAtoms::Connection, "close");

    return NS_OK;
}

