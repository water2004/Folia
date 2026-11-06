const config=(value,source)=>`%%{init: ${JSON.stringify({journey:value})}}%%\n${source}`;
export const cases=[
 ['scores','journey\ntitle Daily work\nsection Start\nPlan: 5: Alice, Bob\nImplement: 3: Alice\nReview: 1: Bob'],
 ['sections','journey\nsection First\nOne: 4: Team\nTwo: 2: Team\nsection Second\nThree: 5: Team\nsection First\nFour: 3: Team'],
 ['no-actors','journey\nsection Tasks\nRead: 4\nWrite: 2'],
 ['long-actors',config({maxLabelWidth:90},'journey\nsection Work\nTask: 4: A person with a long display name, Supercalifragilisticexpialidocious')],
 ['breaks','journey\nsection First<br>part\nLine one<br/>Line two: 4: Reader'],
 ['configured',config({width:180,height:60,diagramMarginX:30,diagramMarginY:15,taskMargin:40,leftMargin:120,maxLabelWidth:90,taskFontSize:16,taskFontFamily:'Arial',titleFontSize:22,titleFontFamily:'Arial',titleColor:'#884400'},'journey\ntitle Editor path\naccTitle: Editing\naccDescr: Preview and save\nsection Edit\nInput: 5: Editor\nSave: 3: Editor')],
 ['missing-data','journey\nTask',null],
 ['empty-score','journey\nTask:',null],
];
