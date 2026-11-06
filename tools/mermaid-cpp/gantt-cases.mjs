const source=(body,format='YYYY-MM-DD')=>`gantt\ndateFormat ${format}\ntodayMarker off\n${body}`;
export const cases=[
 ['dates',source('title Delivery plan\nDesign :design, 2025-01-06, 3d\nBuild :build, 2025-01-09, 2025-01-15\nShip :2025-01-15, 1d')],
 ['dependencies',source('First :first, 2025-02-03, 2d\nSecond :second, after first, 3d\nParallel :parallel, 2025-02-03, until second\nFinish :after first second, 1d')],
 ['statuses',source('Completed :done, a, 2025-03-03, 2d\nCurrent :active, crit, b, after a, 3d\nReview :crit, c, after b, 1d\nLaunch :milestone, m, after c, 0d')],
 ['sections',source('section Prepare\nResearch :2025-04-01, 3d\nPlan :2025-04-04, 2d\nsection Deliver\nImplement :2025-04-06, 4d\nReview :2025-04-10, 1d')],
 ['weekends',source('excludes weekends\nincludes 2025-01-11\nWork :w, 2025-01-09, 4d\nNext :after w, 2d')],
 ['hours',source('axisFormat %H:%M\ntickInterval 2hour\nPrepare :2025-05-01 08:00, 2h\nWork :2025-05-01 10:00, 4h\nCheck :2025-05-01 14:00, 90m','YYYY-MM-DD HH:mm')],
 ['compact','%%{init: {"gantt":{"displayMode":"compact"}}}%%\n'+source('section Team\nOne :2025-06-02, 2d\nTwo :2025-06-02, 3d\nThree :2025-06-05, 1d')],
 ['axis','%%{init: {"gantt":{"topAxis":true}}}%%\n'+source('title Milestones\naccTitle: Work schedule\naccDescr: Inclusive dates\ninclusiveEndDates\naxisFormat %m-%d\ntickInterval 2day\nFirst :2025-07-01, 2025-07-03\nSecond :2025-07-04, 2025-07-08')],
 ['invalid-date',source('Task :not-a-date, 3d'),null],
 ['missing-data',source('Task'),null],
];
