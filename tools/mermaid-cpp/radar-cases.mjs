export const cases = [
 ['basic','radar-beta\naxis A,B,C\ncurve current{1,2,3}'],
 ['labels','radar-beta:\ntitle Document quality\naxis A["Readability"],B["Structure"],C["Completeness"],D["Accuracy"]\ncurve current["Current version"]{4,3,2,4},target["Target"]{5,5,5,5}'],
 ['references','radar-beta\naxis A,B,C\ncurve current{C:3,A:1,B:2}\ncurve target{B 4,C 5,A 3}'],
 ['polygon-clipping','radar-beta\naxis A,B,C,D\ncurve current{0,3,7,10}\nmin 2,max 8,ticks 4,graticule polygon,showLegend false'],
 ['configured','%%{init: {"radar": {"width": 400, "height": 300, "marginTop": 80, "marginRight": 100, "marginBottom": 60, "marginLeft": 70, "axisScaleFactor": 1.1, "axisLabelFactor": 1.2, "curveTension": 0.1}, "themeVariables": {"radar": {"axisColor": "#ff0000", "curveOpacity": 0.3}}}}%%\nradar-beta\naxis A,B,C\ncurve current{3,4,2}\nmax 5'],
 ['metadata','---\ntitle: 项目评估\n---\nradar-beta\naccTitle: Quality\naccDescr {\nThree dimensions\nfor the document\n}\naxis A["质量"],B["结构"],C["完整性 😀"]\ncurve result["结果"]{1,2,3}'],
 ['short-curve','radar-beta\naxis A,B,C\ncurve incomplete{1,2},complete{1,2,3}'],
 ['empty','radar-beta'],
 ['missing-reference','radar-beta\naxis A,B,C\ncurve incomplete{A:1,B:2}',null],
 ['unfinished-curve','radar-beta\naxis A,B,C\ncurve current{1,',null],
 ['unknown-axis','radar-beta\naxis A,B,C\ncurve current{A:1,B:2,Unknown:3}',null],
];
