module.exports = [
  {
    "type": "heading",
    "defaultValue": "Energie Tafief Configuration"
  },
  {
    "type": "text",
    "defaultValue": "Je kunt hier aangeven met welke kleuren je de gegevens getoond wilt hebben en hoe je de tarieven wilt berekenen op basis van de kale uur prijs."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Colors"
      },
      {
        "type": "color",
        "messageKey": "BackgroundColor",
        "defaultValue": "0x000000",
        "label": "Achtergrond kleur"
      },
      {
        "type": "color",
        "messageKey": "TextColor",
        "defaultValue": "0xFFFFFF",
        "label": "Tekst kleur"
      },
      {
        "type": "color",
        "messageKey": "ForegroundColorPast",
        "defaultValue": "0x004400",
        "label": "Historische uren",
        "allowGray": true
      },
      {
        "type": "color",
        "messageKey": "ForegroundColorFuture",
        "defaultValue": "0x008800",
        "label": "Toekomstige uren",
        "allowGray": true
      },
      {
        "type": "color",
        "messageKey": "HighlightColor",
        "defaultValue": "0x00FF00",
        "label": "Selectie kleur",
        "allowGray": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Tarief berekening"
      },
      {
        "type": "input",
        "messageKey": "InkoopVergoeding",
        "label": "Inkoop vergoeding €",
        "description": "Inclusief BTW. E.g. 0.02",
        "defaultValue": "0.02"
      },
      {
        "type": "input",
        "messageKey": "EnergieBelasting",
        "label": "Energie belasting €",
        "description": "2026 = 0.1108",
        "defaultValue": "0.1108"
      },
      {
        "type": "input",
        "messageKey": "BTW",
        "label": "BTW %",
        "description": "2026 = 21",
        "defaultValue": "21"
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];