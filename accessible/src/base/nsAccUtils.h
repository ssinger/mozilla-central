/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAccUtils_h_
#define nsAccUtils_h_

#include "nsIAccessible.h"
#include "nsIAccessibleRole.h"
#include "nsIAccessibleText.h"

#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"

#include "mozilla/dom/Element.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPersistentProperties2.h"
#include "nsIPresShell.h"
#include "nsPoint.h"

class nsAccessNode;
class Accessible;
class HyperTextAccessible;
class DocAccessible;
struct nsRoleMapEntry;

class nsAccUtils
{
public:
  /**
   * Returns value of attribute from the given attributes container.
   *
   * @param aAttributes - attributes container
   * @param aAttrName - the name of requested attribute
   * @param aAttrValue - value of attribute
   */
  static void GetAccAttr(nsIPersistentProperties *aAttributes,
                         nsIAtom *aAttrName,
                         nsAString& aAttrValue);

  /**
   * Set value of attribute for the given attributes container.
   *
   * @param aAttributes - attributes container
   * @param aAttrName - the name of requested attribute
   * @param aAttrValue - new value of attribute
   */
  static void SetAccAttr(nsIPersistentProperties *aAttributes,
                         nsIAtom *aAttrName,
                         const nsAString& aAttrValue);

  /**
   * Set group attributes ('level', 'setsize', 'posinset').
   */
  static void SetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                               int32_t aLevel, int32_t aSetSize,
                               int32_t aPosInSet);

  /**
   * Get default value of the level for the given accessible.
   */
  static int32_t GetDefaultLevel(Accessible* aAcc);

  /**
   * Return ARIA level value or the default one if ARIA is missed for the
   * given accessible.
   */
  static int32_t GetARIAOrDefaultLevel(Accessible* aAccessible);

  /**
   * Compute group level for nsIDOMXULContainerItemElement node.
   */
  static int32_t GetLevelForXULContainerItem(nsIContent *aContent);

  /**
   * Set container-foo live region attributes for the given node.
   *
   * @param aAttributes    where to store the attributes
   * @param aStartContent  node to start from
   * @param aTopContent    node to end at
   */
  static void SetLiveContainerAttributes(nsIPersistentProperties *aAttributes,
                                         nsIContent *aStartContent,
                                         nsIContent *aTopContent);

  /**
   * Any ARIA property of type boolean or NMTOKEN is undefined if the ARIA
   * property is not present, or is "" or "undefined". Do not call 
   * this method for properties of type string, decimal, IDREF or IDREFS.
   * 
   * Return true if the ARIA property is defined, otherwise false
   */
  static bool HasDefinedARIAToken(nsIContent *aContent, nsIAtom *aAtom);

  /**
   * Return atomic value of ARIA attribute of boolean or NMTOKEN type.
   */
  static nsIAtom* GetARIAToken(mozilla::dom::Element* aElement, nsIAtom* aAttr);

  /**
   * Return document accessible for the given DOM node.
   */
  static DocAccessible* GetDocAccessibleFor(nsINode* aNode)
  {
    nsIPresShell *presShell = nsCoreUtils::GetPresShellFor(aNode);
    return GetAccService()->GetDocAccessible(presShell);
  }

  /**
   * Return document accessible for the given docshell.
   */
  static DocAccessible* GetDocAccessibleFor(nsIDocShellTreeItem* aContainer)
  {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
    nsCOMPtr<nsIPresShell> presShell;
    docShell->GetPresShell(getter_AddRefs(presShell));
    return GetAccService()->GetDocAccessible(presShell);
  }

  /**
    * Return ancestor in this document with the given role if it exists.
    *
    * @param  aDescendant  [in] descendant to start search with
    * @param  aRole        [in] role to find matching ancestor for
    * @return               the ancestor accessible with the given role, or
    *                       nullptr if no match is found
    */
   static Accessible* GetAncestorWithRole(Accessible* aDescendant,
                                          uint32_t aRole);

  /**
   * Return single or multi selectable container for the given item.
   *
   * @param  aAccessible  [in] the item accessible
   * @param  aState       [in] the state of the item accessible
   */
  static Accessible* GetSelectableContainer(Accessible* aAccessible,
                                            uint64_t aState);

  /**
   * Return true if the DOM node of given accessible has aria-selected="true"
   * attribute.
   */
  static bool IsARIASelected(Accessible* aAccessible);

  /**
   * Return text accessible containing focus point of the given selection.
   * Used for normal and misspelling selection changes processing.
   *
   * @param aSelection  [in] the given selection
   * @return            text accessible
   */
  static HyperTextAccessible*
    GetTextAccessibleFromSelection(nsISelection* aSelection);

  /**
   * Converts the given coordinates to coordinates relative screen.
   *
   * @param aX               [in] the given x coord
   * @param aY               [in] the given y coord
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessNode      [in] the accessible if coordinates are given
   *                         relative it.
   * @param aCoords          [out] converted coordinates
   */
  static nsresult ConvertToScreenCoords(int32_t aX, int32_t aY,
                                        uint32_t aCoordinateType,
                                        nsAccessNode *aAccessNode,
                                        nsIntPoint *aCoords);

  /**
   * Converts the given coordinates relative screen to another coordinate
   * system.
   *
   * @param aX               [in, out] the given x coord
   * @param aY               [in, out] the given y coord
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessNode      [in] the accessible if coordinates are given
   *                         relative it
   */
  static nsresult ConvertScreenCoordsTo(int32_t *aX, int32_t *aY,
                                        uint32_t aCoordinateType,
                                        nsAccessNode *aAccessNode);

  /**
   * Returns coordinates relative screen for the top level window.
   *
   * @param aAccessNode  the accessible hosted in the window
   */
  static nsIntPoint GetScreenCoordsForWindow(nsAccessNode *aAccessNode);

  /**
   * Returns coordinates relative screen for the parent of the given accessible.
   *
   * @param aAccessNode  the accessible
   */
  static nsIntPoint GetScreenCoordsForParent(nsAccessNode *aAccessNode);

  /**
   * Return the role of the given accessible.
   */
  static uint32_t Role(nsIAccessible *aAcc)
  {
    uint32_t role = nsIAccessibleRole::ROLE_NOTHING;
    if (aAcc)
      aAcc->GetRole(&role);

    return role;
  }

  /**
   * Get the ARIA attribute characteristics for a given ARIA attribute.
   * 
   * @param aAtom  ARIA attribute
   * @return       A bitflag representing the attribute characteristics
   *               (see nsARIAMap.h for possible bit masks, prefixed "ARIA_")
   */
  static uint8_t GetAttributeCharacteristics(nsIAtom* aAtom);

  /**
   * Get the 'live' or 'container-live' object attribute value from the given
   * ELiveAttrRule constant.
   *
   * @param  aRule   [in] rule constant (see ELiveAttrRule in nsAccMap.h)
   * @param  aValue  [out] object attribute value
   *
   * @return         true if object attribute should be exposed
   */
  static bool GetLiveAttrValue(uint32_t aRule, nsAString& aValue);

#ifdef DEBUG
  /**
   * Detect whether the given accessible object implements nsIAccessibleText,
   * when it is text or has text child node.
   */
  static bool IsTextInterfaceSupportCorrect(Accessible* aAccessible);
#endif

  /**
   * Return true if the given accessible has text role.
   */
  static bool IsText(nsIAccessible *aAcc)
  {
    uint32_t role = Role(aAcc);
    return role == nsIAccessibleRole::ROLE_TEXT_LEAF ||
           role == nsIAccessibleRole::ROLE_STATICTEXT;
  }

  /**
   * Return text length of the given accessible, return 0 on failure.
   */
  static uint32_t TextLength(Accessible* aAccessible);

  /**
   * Return true if the given accessible is embedded object.
   */
  static bool IsEmbeddedObject(nsIAccessible *aAcc)
  {
    uint32_t role = Role(aAcc);
    return role != nsIAccessibleRole::ROLE_TEXT_LEAF &&
           role != nsIAccessibleRole::ROLE_WHITESPACE &&
           role != nsIAccessibleRole::ROLE_STATICTEXT;
  }

  /**
   * Transform nsIAccessibleStates constants to internal state constant.
   */
  static inline uint64_t To64State(uint32_t aState1, uint32_t aState2)
  {
    return static_cast<uint64_t>(aState1) +
        (static_cast<uint64_t>(aState2) << 31);
  }

  /**
   * Transform internal state constant to nsIAccessibleStates constants.
   */
  static inline void To32States(uint64_t aState64,
                                uint32_t* aState1, uint32_t* aState2)
  {
    *aState1 = aState64 & 0x7fffffff;
    if (aState2)
      *aState2 = static_cast<uint32_t>(aState64 >> 31);
  }

  /**
   * Return true if the given accessible can't have children. Used when exposing
   * to platform accessibility APIs, should the children be pruned off?
   */
  static bool MustPrune(Accessible* aAccessible);
};

#endif
