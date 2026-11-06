const config=(value,source)=>`%%{init: ${JSON.stringify({timeline:value})}}%%\n${source}`;
export const cases=[
 ['events','timeline\ntitle Development\n2024 : Design : Prototype\n2025 : Build\n2026 : Release'],
 ['sections','timeline\nsection Planning\nJanuary : Draft\nFebruary : Review\nsection Delivery\nMarch : Ship : Support'],
 ['periods','timeline\nFirst\nSecond : Event'],
 ['wrapped','timeline\nsection A section with a long heading\nA long period heading : First line<br>second line : An event description that needs several lines'],
 ['monochrome',config({disableMulticolor:true},'timeline\nOne : Alpha\nTwo : Beta\nThree : Gamma')],
 ['configured',config({leftMargin:80,padding:20},'timeline TD\ntitle Milestones\naccTitle: Schedule\naccDescr: Main milestones\nsection Start\nDay 1 : Open\nDay 2 : Finish')],
 ['orphan-event','timeline\n: Event',null],
 ['empty-event','timeline\nPeriod :',null],
];
