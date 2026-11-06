export const cases = [
  ['basic', 'eventmodeling\ntf 01 ui UI\ntf 02 cmd RunAction\ntf 03 evt ActionExecuted'],
  ['qualified', 'eventmodeling\ntf 01 ui Cart.UI\ntf 02 cmd Inventory.AddItem ->> 01\ntf 03 evt Inventory.ItemAdded ->> 02'],
  ['data', 'eventmodeling\ntf 01 ui UI\ntf 02 cmd AddItem { productId: 7 }\ntf 03 evt ItemAdded [[ItemData]]\ndata ItemData\n{\n productId: 7\n}'],
  ['reset', 'eventmodeling\ntf 01 ui UI\ntf 02 cmd Run\nrf 03 evt Reset'],
  ['relations', 'eventmodeling\ntf 01 ui Cart\ntf 02 cmd Add\ntf 03 cmd Remove\ntf 04 evt Changed ->> 03 ->> 02\nrf 05 rmo CartView ->> 04\ntf 06 processor Update ->> 05'],
  ['namespaces', 'eventmodeling\ntf 01 ui Catalog.Cart\ntf 02 ui Catalog.Checkout\ntf 03 cmd Catalog.PlaceOrder\ntf 04 event Catalog.OrderPlaced\ntf 05 readmodel Catalog.CartView\ntf 06 ui Shop.Cart.Payment'],
  ['formatted-payload', 'eventmodeling\ntf 01 evt Output `html` { <script>ignore()</script><em onclick="ignored">Paid</em> &amp; <strong>Ready</strong><br><sup>2</sup> }'],
];
export const invalid = [
  'eventmodeling\ntf 01 unknown Bad',
  'eventmodeling\ntf 01 ui A\nrf 01 evt B',
  'eventmodeling\ngwt 01 given ui A',
];
