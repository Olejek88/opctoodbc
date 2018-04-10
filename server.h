//-----------------------------------------------------------------------------
#include <windows.h>
#include "unilog.h"
#include "opcda.h"
#include <time.h>
//-----------------------------------------------------------------------------
#define LOGID logg,0				// log identifiner
#define LOG_FNAME	"odbcopc.log"	// log name
#define CFG_FILE	"odbcopc.ini"	// cfg name
#define MAX_DATABASE_NUM	5		// maximum number of database
#define MAX_TAGS_PER_BASE	100		// maximum number of parametrs on one database
#define TAGS_NUM_MAX MAX_DATABASE_NUM*MAX_TAGS_PER_BASE		// maximum number of tags
#define DATALEN_MAX 120				// maximum lenght of the tags
//-----------------------------------------------------------------------------
typedef struct _DatabaseR DatabaseR;	// database info
struct _DatabaseR {  
  CHAR	odbcname[30];	// database sourcename
  CHAR	login[20];		// database login
  CHAR	pass[20];		// database password
  UINT	numtables;		// number of tables in database
};
DatabaseR Database [MAX_DATABASE_NUM];
//-----------------------------------------------------------------------------
typedef struct _DataR DataR;	// channels info

struct _DataR {
  SHORT db;			// порядковый номер базы
  UINT  tags;		// порядковый номер тега
  CHAR  table[20];	// название таблицы
  CHAR  name[DATALEN_MAX];	// имя тега
  UINT	fields[20];		// поля-критерии выборки
  CHAR	fieldz[20][50];	// поля-критерии выборки
  CHAR	key[50];		// поле предварительного отбора
  SHORT nstatus;		// поле статуса
  SHORT ndate;			// поле даты
  SHORT nvalue;			// поле значения
  CHAR  value[100];		// текущее значение.
  SHORT type;			// тип измеряемой величины (1-float, 2-int, 3-text..)
  SHORT rights;			// права тега 1-чтение, 2-запись, 3-чтение/запись
};

DataR DR[TAGS_NUM_MAX];