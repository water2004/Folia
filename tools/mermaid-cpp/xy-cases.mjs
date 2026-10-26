// Fixed functional corpus for the next native XY package. null = parse failure.
const config=(value,source)=>`%%{init: ${JSON.stringify(value)}}%%\n${source}`;
export const cases=[
 ['category-bars','xychart-beta\ntitle Monthly output\nx-axis Month [Jan, Feb, Mar]\ny-axis Count 0 --> 100\nbar [20, 60, 40]'],
 ['numeric-line','xychart-beta\nx-axis Time 0 --> 10\ny-axis Value -10 --> 10\nline [-5, 0, 8, 3]'],
 ['automatic','xychart-beta\nbar [5, 12, 8]\nline [7, 9, 11]'],
 ['horizontal','xychart-beta horizontal\ntitle "Horizontal"\nx-axis [One, Two, Three]\ny-axis Amount 0 --> 30\nbar [10, 25, 20]\nline [12, 20, 18]'],
 ['legend',config({xyChart:{showLegend:true}},'xychart-beta\nx-axis [A, B, C]\nbar Sales [10, 30, 20]\nline Trend [15, 25, 18]')],
 ['configured',config({xyChart:{width:640,height:400,titleFontSize:24,plotReservedSpacePercent:60,xAxis:{showTitle:false,labelFontSize:14,tickLength:8},yAxis:{showTick:false,showAxisLine:false}},themeVariables:{xyChart:{plotColorPalette:'#cc3300,#0066aa'}}},'xychart-beta\ntitle Configured\nx-axis Month [Jan, Feb, Mar]\ny-axis Total 0 --> 40\nbar [10, 30, 20]\nline [15, 25, 18]')],
 ['labels-vertical',config({xyChart:{showDataLabel:true}},'xychart-beta\nx-axis [A, B, C]\ny-axis 0 --> 100\nbar [20, 60, 40]')],
 ['labels-horizontal',config({xyChart:{showDataLabel:true,showDataLabelOutsideBar:true}},'xychart-beta horizontal\nx-axis [A, B, C]\ny-axis 0 --> 100\nbar [20, 60, 40]')],
 ['point-labels','xychart-beta\nx-axis [A, B, C]\nline [20 "First", 60, 40 "Last"]'],
 ['negative','xychart-beta\nx-axis [A, B, C]\ny-axis -20 --> 20\nbar [-10, 0, 15]'],
 ['truncated','xychart-beta\nx-axis [A, B]\nbar [2, 5, 100]'],
 ['unicode-metadata','---\ntitle: 项目统计\n---\nxychart-beta\naccTitle: Chart\naccDescr {\nEditor output\nby month\n}\nx-axis ["一月", "二月", "三月"]\nline [3, 7, 5]'],
 ['unfinished-plot','xychart-beta\nbar [1,',null],
 ['bad-axis','xychart-beta\ny-axis 0 -->\nbar [1, 2]',null],
 ['bad-value','xychart-beta\nbar [1, unknown]',null],
];
