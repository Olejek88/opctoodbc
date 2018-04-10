#define _CRT_SECURE_NO_WARNINGS 1

#define _WIN32_DCOM				// Enables DCOM extensions
#define INITGUID				// Initialize OLE constants
#define ECL_SID "opc.fromodbc"	// identificator of OPC server

#include <stdio.h>
#include <math.h>				// some mathematical function
#include "server.h"				// server variable
#include "unilog.h"				// universal utilites for creating log-files
#include <locale.h>				// set russian codepage
#include "opcda.h"				// basic function for OPC:DA
#include "lightopc.h"			// light OPC library header file

#include <sql.h>
#include <sqlext.h>
#include <stdlib.h>
#include <mbstring.h>
#include "dbaccess.h"
//---------------------------------------------------------------------------------
static const loVendorInfo vendor = {0,5,1,0,"ODBC to OPC Server" };	// OPC vendor info (Major/Minor/Build/Reserv)
static void a_server_finished(VOID*, loService*, loClient*);		// OnServer finish his work
static int OPCstatus=OPC_STATUS_RUNNING;							// status of OPC server
loService *my_service;				// name of light OPC Service
//dbaccess dbase[MAX_DATABASE_NUM];	// Database pointer
dbaccess dbase[MAX_DATABASE_NUM];	// Database pointer
//---------------------------------------------------------------------------------
UINT	res=0;							// SQL request result
WCHAR	bufMB[2501];					//
UINT	tag_nums[MAX_DATABASE_NUM]={0};	// 
UINT	dbNum=0;						// database nums
UINT	tag_num=0;						// tags counter
//---------------------------------------------------------------------------------
unilog *logg=NULL;				// new structure of unilog
FILE	*CfgFile;				// pointer to .ini file
//---------------------------------------------------------------------------------
BOOL ReadData (UINT tag);		// read data from DB
INT	 init_tags(VOID);			// Init tags
UINT DestroyDriver();			//
UINT InitDriver();				//
VOID poll_device();				//
CHAR *absPath(CHAR *fileName);	//
//---------------------------------------------------------------------------------
static CRITICAL_SECTION lk_values;	// protects ti[] from simultaneous access 
static INT mymain(HINSTANCE hInstance, INT argc, CHAR *argv[]);
static INT show_error(LPCTSTR msg);		// just show messagebox with error
static INT show_msg(LPCTSTR msg);		// just show messagebox with message
CHAR* ReadParam (CHAR *SectionName,CHAR *Value);	// read parametr from .ini file
CRITICAL_SECTION drv_access;
BOOL WorkEnable=TRUE;
//---------------------------------------------------------------------------------
CHAR	argv0[FILENAME_MAX + 32];	// lenght of command line (file+path (260+32))
static CHAR *tn[TAGS_NUM_MAX];		// Tag name
static loTagValue tv[TAGS_NUM_MAX];	// Tag value
static loTagId ti[TAGS_NUM_MAX];	// Tag id
CHAR	tmp[200];				// temporary array for strings
CHAR	dataset[2000];			// temporary array for data
CHAR	query[500];				// temporary array for DB query
//---------------------------------------------------------------------------------
// {E42E22A3-9374-4c14-92AF-8BE77164F95C}
DEFINE_GUID(GID_ODBCtoOPCDll, 
0xe42e22a3, 0x9374, 0x4c14, 0x92, 0xaf, 0x8b, 0xe7, 0x71, 0x64, 0xf9, 0x5c);
// {A9977F6C-2A9D-4d6a-95AA-8878AF825A29}
DEFINE_GUID(GID_ODBCtoOPCExe, 
0xa9977f6c, 0x2a9d, 0x4d6a, 0x95, 0xaa, 0x88, 0x78, 0xaf, 0x82, 0x5a, 0x29);
//---------------------------------------------------------------------------------
inline void cleanup_common(void)	// delete log-file
{ UL_INFO((LOGID, "Finish ODBC to OPC Server"));  
  unilog_Delete(logg); logg = NULL;
  UL_INFO((LOGID, "Total Finish")); }
inline void init_common(void)		// create log-file
{ logg = unilog_Create(ECL_SID, absPath(LOG_FNAME), NULL, 0, ll_DEBUG); // level [ll_FATAL...ll_DEBUG] 
   UL_INFO((LOGID, "Start ODBC to OPC Server v0.2.5")); printf ("Start ODBC to OPC Server\n");}

INT show_error(LPCTSTR msg)			// just show messagebox with error
{ ::MessageBox(NULL, msg, ECL_SID, MB_ICONSTOP|MB_OK);
  return 1;}
INT show_msg(LPCTSTR msg)			// just show messagebox with message
{ ::MessageBox(NULL, msg, ECL_SID, MB_OK);
  return 1;}
//---------------------------------------------------------------------------------
inline void cleanup_all(DWORD objid)
{ // Informs OLE that a class object, previously registered is no longer available for use  
  if (FAILED(CoRevokeClassObject(objid)))  UL_WARNING((LOGID, "CoRevokeClassObject() failed..."));
  DestroyDriver();					// destroy driver
  CoUninitialize();					// Closes the COM library on the current thread
  cleanup_common();					// delete log-file
}
//---------------------------------------------------------------------------------
#include "opc_main.h"	//	main server 
//-----------------------------------------------------------------------------------
CHAR *absPath(CHAR *fileName)					// return abs path of file
{ static char path[sizeof(argv0)]="\0";			// path - massive of comline
  CHAR *cp;
  if(*path=='\0') strncpy(path, argv0, 255);
  if(NULL==(cp=strrchr(path,'\\'))) cp=path; else cp++;
  cp=strncpy(cp,fileName,255);
  return path;}
//---------------------------------------------------------------------------------
INT APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,INT nCmdShow)
{ static char *argv[3] = { "dummy.exe", NULL, NULL };	// defaults arguments
  argv[1] = lpCmdLine;									// comandline - progs keys
  return mymain(hInstance, 2, argv);}
INT main(INT argc, CHAR *argv[])
{  return mymain(GetModuleHandle(NULL), argc, argv); }
//---------------------------------------------------------------------------------
INT mymain(HINSTANCE hInstance, INT argc, CHAR *argv[]) 
{
 const char eClsidName [] = ECL_SID;			// desription 
 const char eProgID [] = ECL_SID;				// name
 CHAR *cp; DWORD objid;
 objid=::GetModuleFileName(NULL, argv0, sizeof(argv0));	// function retrieves the fully qualified path for the specified module
 if(objid==0 || objid+50 > sizeof(argv0)) return 0;		// not in border
 init_common();									// create log-file
 if(NULL==(cp = setlocale(LC_ALL, ".1251")))	// sets all categories, returning only the string cp-1251
	{ 
	 UL_ERROR((LOGID, "setlocale() - Can't set 1251 code page"));	// in bad case write error in log
	 cleanup_common();							// delete log-file
     return 0;
	}
 INT finish=1;		// flag of comlection
 cp = argv[1];		
 if(cp)				// check keys of command line 
	{     
     if (strstr(cp, "/r"))	//	attempt registred server
		{
	     if (loServerRegister(&GID_ODBCtoOPCExe, eProgID, eClsidName, argv0, 0)) 
			{ show_error("Registration Failed"); UL_ERROR((LOGID, "Registration <%s> <%s> Failed", eProgID, argv0));  } 
		 else
			{ show_msg("ODBC to OPC Registration Ok"); UL_INFO((LOGID, "Registration <%s> <%s> Ok", eProgID, argv0));  }
		} 
	else 
		if (strstr(cp, "/u")) 
			{
			 if (loServerUnregister(&GID_ODBCtoOPCExe, eClsidName)) 
				{ show_error("UnRegistration Failed"); UL_ERROR((LOGID, "UnReg <%s> <%s> Failed", eClsidName, argv0));  } 
			 else 
				{ show_msg("ODBC to OPC Server Unregistered"); UL_INFO((LOGID, "UnReg <%s> <%s> Ok", eClsidName, argv0));}
			} 
		else  // only /r and /u options
			if (strstr(cp, "/?")) 
				 show_msg("Use: \nKey /r to register server.\nKey /u to unregister server.\nKey /? to show this help.");
				 else
					{
					 UL_WARNING((LOGID, "Ignore unknown option <%s>", cp));
					 finish = 0;		// nehren delat
					}
		if (finish) {      cleanup_common();      return 0;    } 
	}
if ((CfgFile = fopen(CFG_FILE, "r+")) == NULL)
	{	
	 show_error("Error open .ini file");
	 UL_ERROR((LOGID, "Error open .ini file"));	// in bad case write error in log
	 return 0;
	}
if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) 
	{	// Initializes the COM library for use by the calling thread
     UL_ERROR((LOGID, "CoInitializeEx() failed. Exiting..."));
     cleanup_common();	// close log-file
     return 0;
	}
UL_INFO((LOGID, "CoInitializeEx() Ok...."));	// write to log
if (InitDriver()) {		// open and set com-port
    CoUninitialize();	// Closes the COM library on the current thread
    cleanup_common();	// close log-file
    return 0;
  }
UL_INFO((LOGID, "InitDriver() Ok...."));	// write to log
UL_INFO((LOGID, "CoRegisterClassObject(%ul, %d, %d)",GID_ODBCtoOPCExe, my_CF, objid));
if (FAILED(CoRegisterClassObject(GID_ODBCtoOPCExe, &my_CF, 
				   CLSCTX_LOCAL_SERVER|CLSCTX_REMOTE_SERVER|CLSCTX_INPROC_SERVER, 
				   REGCLS_MULTIPLEUSE, &objid)))
    { UL_ERROR((LOGID, "CoRegisterClassObject() failed. Exiting..."));
      cleanup_all(objid);		// close comport and unload all librares
      return 0; }
UL_INFO((LOGID, "CoRegisterClassObject() Ok...."));	// write to log

Sleep(1000);
my_CF.Release();		// avoid locking by CoRegisterClassObject() 
if (OPCstatus!=OPC_STATUS_RUNNING)	// ???? maybe Status changed and OPC not currently running??
	{	while(my_CF.in_use()) Sleep(1000);	// wait
		cleanup_all(objid);
		return 0;	}
while(my_CF.in_use())					// while server created or client connected
	{
	 if (WorkEnable) poll_device();		// polling devices else do nothing (and be nothing)	 
	}
UL_INFO((LOGID, "end cleanup_all()"));	// write to log
cleanup_all(objid);						// destroy himself
return 0;
}
//-------------------------------------------------------------------
loTrid ReadTags(const loCaller *, unsigned  count, loTagPair taglist[],
		VARIANT   values[],	WORD      qualities[],	FILETIME  stamps[],
		HRESULT   errs[],	HRESULT  *master_err,	HRESULT  *master_qual,
		const VARTYPE vtype[],	LCID lcid)
{  return loDR_STORED; }
//-------------------------------------------------------------------
INT WriteTags(const loCaller *ca,
              unsigned count, loTagPair taglist[],
              VARIANT values[], HRESULT error[], HRESULT *master, LCID lcid)
{  return loDW_TOCACHE; }
//-------------------------------------------------------------------
VOID activation_monitor(const loCaller *ca, INT count, loTagPair *til){}
//-------------------------------------------------------------------
UINT InitDriver()
{
 CHAR host[100],login[100],pass[100],database[100];
 UL_INFO((LOGID, "ODBC to OPC InitDriver()")); printf ("ODBC to OPC InitDriver()\n");
 loDriver ld;		// structure of driver description
 LONG ecode;		// error code 
 //CHAR	name[100];	// device name
 //tTotal = TAGS_NUM_MAX;		// total tag quantity
 if (my_service) {	
      UL_ERROR((LOGID, "Driver already initialized!"));
      return 0;
  }
 memset(&ld, 0, sizeof(ld));   
 ld.ldRefreshRate =5000;		// polling time 
 ld.ldRefreshRate_min = 4000;	// minimum polling time
 ld.ldWriteTags = WriteTags;	// pointer to function write tag
 ld.ldReadTags = ReadTags;		// pointer to function read tag
 ld.ldSubscribe = activation_monitor;	// callback of tag activity
 ld.ldFlags = loDF_IGNCASE;				// ignore case
 ld.ldBranchSep = '/';					// hierarchial branch separator
 ecode = loServiceCreate(&my_service, &ld, TAGS_NUM_MAX);		//	creating loService 
 UL_TRACE((LOGID, "%!e loServiceCreate()=", ecode));	// write to log returning code
 if (ecode) return 1;									// error to create service	
 InitializeCriticalSection(&lk_values);
 EnterCriticalSection(&lk_values); 
 for (UINT db=0;db<MAX_DATABASE_NUM;db++)
	{
	 sprintf (database,"database%d",db+1);
	 sprintf (host,"%s",ReadParam (database,"name"));
	 sprintf (login,"%s",ReadParam (database,"login"));
	 sprintf (pass,"%s",ReadParam (database,"pass"));	 
	 if (!strlen(host)) continue;
	 UL_ERROR((LOGID, "Attempt connect to database on host [%s] with login: [%s] | pass: [%s]",host,login,pass)); 
	 printf ("Attempt connect to database on host [%s] with login: %s | pass: %s\n",host,login,pass);
	 if (strlen(host)<=0 || strlen(login)<=0) continue;
	 dbase[0].dblogon();
	 dbase[db].dblogon();	 
	 if (!dbase[db].sqlconn((UCHAR FAR *) host,(UCHAR FAR *) login,(UCHAR FAR *) pass))
		{ UL_ERROR((LOGID, "Cannot connect to host!")); printf ("Cannot connect to host!\n"); continue; }
	 else {UL_ERROR((LOGID, "Connect to host success")); printf ("Connect to host success\n");}
	 strcpy (Database[dbNum].login,login);
	 strcpy (Database[dbNum].pass,pass);
	 strcpy (Database[dbNum].odbcname,host); dbNum++;
	 for (UINT i=1;i<MAX_TAGS_PER_BASE;i++)
		{
		 sprintf (database,"database%d tag%d",db+1,i);
		 DR[tag_num].db = db;
		 strcpy (DR[tag_num].name,ReadParam (database,"name"));
		 if (!strlen(DR[tag_num].name)) continue;
		 DR[tag_num].rights=atoi(ReadParam (database,"rights"));
		 DR[tag_num].ndate=atoi(ReadParam (database,"date"));
		 DR[tag_num].nvalue=atoi(ReadParam (database,"value"));
		 DR[tag_num].nstatus=atoi(ReadParam (database,"status"));
		 strcpy (DR[tag_num].table,ReadParam (database,"table"));
		 strcpy (DR[tag_num].key,ReadParam (database,"key"));
		 DR[tag_num].tags=tag_num;
		 DR[tag_num].type=atoi(ReadParam (database,"type"));
		 for (UINT fld=0,nfl=0; fld<=29;fld++)
			{
			 sprintf (argv0,"field%d",fld);			 
			 strcpy(DR[tag_num].fieldz[nfl],ReadParam (database,argv0));
			 if (strlen(DR[tag_num].fieldz[nfl])>0 && strlen(DR[tag_num].fieldz[nfl])<50)
				{
				 DR[tag_num].fields[nfl]=fld;
				 //strcpy(DR[tag_num].fieldz[nfl],ReadParam (database,argv0));
				 nfl++;
				}
			}
		 tag_num++;
		}
	}
 
 UL_INFO((LOGID, "Total %d database found | Tags: %d",dbNum,tag_num)); 
 LeaveCriticalSection(&lk_values);
 if (!dbNum) { UL_ERROR((LOGID, "No database found")); return 1; } 

 if (init_tags())	return 1; 
 else				return 0;
}
//-----------------------------------------------------------------------------------
VOID poll_device()
{
 FILETIME ft;
 INT ecode=0; CHAR field[100];
 UINT	pres=0,i=0;
 DWORD  start=GetTickCount();
 for (UINT i=0;i<tag_num;i++)
 if (DR[i].tags>=0)
	{
	 UL_DEBUG((LOGID, "[%d] (%d) %s [%d|%d|%d] (%d)",DR[i].tags,DR[i].db,DR[i].name,DR[i].nstatus,DR[i].ndate,DR[i].nvalue,DR[i].type));
	 if (strlen(DR[i].key)) sprintf (query,"SELECT * FROM %s WHERE %s",DR[i].table,DR[i].key);
	 else sprintf (query,"SELECT * FROM %s ",DR[i].table);
	 SQLRETURN  retcode=SQLExecDirect (dbase[DR[i].db].hstmnt, (UCHAR *)query, SQL_NTS);
	 UL_INFO((LOGID, "SQLDirect (%s) retcode = %d",query,retcode));
	 if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	 while (TRUE)
			{
			 retcode = SQLFetch(dbase[DR[i].db].hstmnt);
			 if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
				 UINT fld=0; pres=0;
				 while (DR[i].fields[fld]>0)
					{
					 SQLGetData(dbase[DR[i].db].hstmnt, DR[i].fields[fld], SQL_C_CHAR, &field, sizeof (field), NULL);
					 //UL_INFO((LOGID, "DR[%d].fields[%d]=%d (%s)(%s)",i,fld,DR[i].fields[fld],field,DR[i].fieldz[fld]));
					 if (strcmp(field,DR[i].fieldz[fld])) pres++;
					 fld++;
					}
				 if (!pres) 
					{
					 SQLGetData(dbase[DR[i].db].hstmnt, DR[i].nvalue, SQL_C_CHAR, &field, sizeof (field), NULL);
					 strncpy (DR[i].value,field, strlen (field));
					 //UL_INFO((LOGID, "DR[%d].nvalue=%d (%s)",i,DR[i].nvalue,field));
					}
				}
			 else break;
			}
	  SQLCloseCursor(dbase[DR[i].db].hstmnt);
	}
 Sleep (200);
 GetSystemTimeAsFileTime(&ft);
 EnterCriticalSection(&lk_values);
 UL_DEBUG((LOGID, "Data to tag (%d)",tag_num));
 for (UINT ci=0;ci<tag_num; ci++)
	{
	 UL_DEBUG((LOGID, "[%d] ci = %d | v = %s",DR[ci].tags,ci,DR[ci].value));
	 VARTYPE tvVt = tv[ci].tvValue.vt;
	 VariantClear(&tv[ci].tvValue);	  
	 if (DR[ci].type==1)
		{
		 setlocale(LC_NUMERIC, "English");
		 V_R4(&tv[ci].tvValue) = (FLOAT) atof(DR[ci].value);
		}
	 if (DR[ci].type==2)
		 V_I2(&tv[ci].tvValue)=atoi(DR[ci].value);
	 if (DR[ci].type==3)
		{
		 SysFreeString (bufMB);  bufMB[0]=0;
		 if (strlen(DR[ci].value)>1 && DR[ci].value[0]!=0) 
			{
			 LCID lcid = MAKELCID(0x0409, SORT_DEFAULT); // This macro creates a locale identifier from a language identifier. Specifies how dates, times, and currencies are formatted
	 	 	 MultiByteToWideChar(CP_ACP,	// ANSI code page
									  0,	// flags
						   DR[ci].value,	// points to the character string to be converted
				 strlen(DR[ci].value)+1,	// size in bytes of the string pointed to 
									bufMB,	// Points to a buffer that receives the translated string
			 sizeof(bufMB)/sizeof(bufMB[0]));	// function maps a character string to a wide-character (Unicode) string
			}		 
		 V_BSTR(&tv[ci].tvValue) = SysAllocString(bufMB);
		}
	 V_VT(&tv[ci].tvValue) = tvVt;
	 tv[ci].tvState.tsQuality = OPC_QUALITY_GOOD;
	 tv[ci].tvState.tsTime = ft;
	}
 loCacheUpdate(my_service, tag_num, tv, 0);
 LeaveCriticalSection(&lk_values);
 Sleep(100);
}
//-------------------------------------------------------------------
UINT DestroyDriver()
{
  if (my_service)		
    {
      INT ecode = loServiceDestroy(my_service);
      UL_INFO((LOGID, "%!e loServiceDestroy(%p) = ", ecode));	// destroy derver
      DeleteCriticalSection(&lk_values);						// destroy CS
      my_service = 0;		
    }
 UL_INFO((LOGID, "Destroy Driver"));
 return	1;
}
//-----------------------------------------------------------------------------------
CHAR* ReadParam (CHAR *SectionName,CHAR *Value)
{
 CHAR buf[150], string1[50], string2[50]; CHAR ret[150]={0};
 CHAR *pbuf=buf; CHAR *bret=ret;
 UINT s_ok=0;
 sprintf(string1,"[%s]",SectionName);
 sprintf(string2,"%s=",Value);
 //UL_INFO((LOGID, "%s %s [%d]",string1,string2,CfgFile));
 rewind(CfgFile);
 while(!feof(CfgFile))
 if(fgets(buf,80,CfgFile)!=NULL)
	{
	 if (strstr(buf,string1))
		{ s_ok=1; break; }
	}
 if (s_ok)
	{
	 while(!feof(CfgFile))
		{
		 if(fgets(buf,100,CfgFile)!=NULL)
			{
			 //UL_INFO((LOGID, "fgets %s",buf));
			 if (strstr(buf,"[")==NULL && strstr(buf,"]")==NULL)
				{
				 //UL_INFO((LOGID, "fgets []",buf));
				 for (s_ok=0;s_ok<strlen(buf)-1;s_ok++) if (buf[s_ok]==';') buf[s_ok+1]='\0';
				 if (strstr(buf,string2))
					{
					 //UL_INFO((LOGID, "%s present",string2));
					 for (s_ok=0;s_ok<strlen(buf)-1;s_ok++)
						if (s_ok>=strlen(string2)) buf[s_ok-strlen(Value)-1]=buf[s_ok];
							 buf[s_ok-strlen(string2)]='\0';					
					 strcpy(ret,buf);
					 //UL_INFO((LOGID, "ret = %s",ret));
					 strncpy (string1,ret,40);
					 return bret;
					}
				}
			  else { buf[0]=0; buf[1]=0; strcpy(ret,buf); return bret; }
			 }
		}	 	
 	 if (SectionName=="Port")	{ buf[0]='1'; buf[1]=0;}
	 buf[0]=0; buf[1]=0;
	 strcpy(ret,buf); return bret;
	}
else{
	 sprintf(buf, "error");			// if something go wrong return error
	 strcpy(ret,buf); return bret;
	}	
}
//------------------------------------------------------------------------------------
INT init_tags(VOID)
{
  UL_INFO((LOGID, "init_tags()"));
  FILETIME ft;		//  64-bit value representing the number of 100-ns intervals since January 1,1601
  UINT rights=0;	// tag type (read/write)
  UINT ecode,i=0;
  WCHAR buf[DATALEN_MAX];
  GetSystemTimeAsFileTime(&ft);	// retrieves the current system date and time
  EnterCriticalSection(&lk_values);
  LCID lcid = MAKELCID(0x0409, SORT_DEFAULT); // This macro creates a locale identifier from a language identifier. Specifies how dates, times, and currencies are formatted
  for (UINT i=0;i<tag_num;i++)
		{
		 DR[i].tags=-1;
		 if (DR[i].rights==1) rights = OPC_READABLE;
		 if (DR[i].rights==2) rights = OPC_WRITEABLE;
		 if (DR[i].rights==3) rights = OPC_READABLE|OPC_WRITEABLE;
		 tn[i] = new char[DATALEN_MAX];	// reserve memory for massive
		 sprintf(tn[i],"%s/%s",Database[DR[i].db].odbcname,DR[i].name);
		 VariantInit(&tv[i].tvValue);
	 	 MultiByteToWideChar(CP_ACP, 0,tn[i], strlen(tn[i])+1,	buf, sizeof(buf)/sizeof(buf[0])); // function maps a character string to a wide-character (Unicode) string
		 if (DR[i].type==1)
			{
			 V_R4(&tv[i].tvValue) = 0.0;
			 V_VT(&tv[i].tvValue) = VT_R4;
			}
		 if (DR[i].type==2)
			{
			 V_I2(&tv[i].tvValue) = 0;
			 V_VT(&tv[i].tvValue) = VT_I2;
			}
		 if (DR[i].type==3)
			{
			 V_BSTR(&tv[i].tvValue) = SysAllocString(L"");
			 V_VT(&tv[i].tvValue) = VT_BSTR;
			}
		 ecode = loAddRealTag_aW(my_service, &ti[i], (loRealTag)(i+1), buf, 0, rights, &tv[i].tvValue, 0, 0);
		 tv[i].tvTi = ti[i];
		 tv[i].tvState.tsTime = ft;
		 tv[i].tvState.tsError = S_OK;
		 tv[i].tvState.tsQuality = OPC_QUALITY_NOT_CONNECTED;
		 UL_TRACE((LOGID, "%!e loAddRealTag(%s) = %u (t=%d)", ecode, tn[i], ti[i], DR[i].type));
		 printf ("loAddRealTag(%s) = %u (t=%d)\n", tn[i], ti[i], DR[i].type);
		 DR[i].tags=ti[i];
		} 
  LeaveCriticalSection(&lk_values);
  for (i=0; i<tag_num; i++) delete tn[i];
  if(ecode) 
  {
    UL_ERROR((LOGID, "%!e driver_init()=", ecode));
    return -1;
  }
  return 0;
}
//-----------------------------------------------------------------------------------
