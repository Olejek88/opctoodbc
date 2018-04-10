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
  SHORT db;			// ���������� ����� ����
  UINT  tags;		// ���������� ����� ����
  CHAR  table[20];	// �������� �������
  CHAR  name[DATALEN_MAX];	// ��� ����
  UINT	fields[20];		// ����-�������� �������
  CHAR	fieldz[20][50];	// ����-�������� �������
  CHAR	key[50];		// ���� ���������������� ������
  SHORT nstatus;		// ���� �������
  SHORT ndate;			// ���� ����
  SHORT nvalue;			// ���� ��������
  CHAR  value[100];		// ������� ��������.
  SHORT type;			// ��� ���������� �������� (1-float, 2-int, 3-text..)
  SHORT rights;			// ����� ���� 1-������, 2-������, 3-������/������
};

DataR DR[TAGS_NUM_MAX];