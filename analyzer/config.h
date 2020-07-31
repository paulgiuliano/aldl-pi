#define ANL_VERSION "1.0"
#define ANL_CONFIGFILE "/etc/aldl/analyzer.conf"

/* configure analysis grids */
#define GRID_RPM_RANGE 7000
#define GRID_RPM_INTERVAL 400
#define GRID_MAP_RANGE 100
#define GRID_MAP_INTERVAL 10
#define GRID_MAF_RANGE 150
#define GRID_MAF_INTERVAL 5

/* reject blm cell 17, so decel events dont fuck up results */
#define REJECTDECEL

/* if rpm is below this, engine is considered not running */
#define MIN_RPM 500

