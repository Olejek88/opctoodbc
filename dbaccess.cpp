#define _CRT_SECURE_NO_WARNINGS 1

#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>
#include <mbstring.h>
#include "dbaccess.h"
#include "unilog.h"				// for log files

#define LOGID1 logg1,0			// id log
#define MYSQLSUCCESS(rc) ((rc==SQL_SUCCESS)||(rc==SQL_SUCCESS_WITH_INFO))
#define LOG_FNAME "dbase2.log"	// file name of log file
#define ECL_SIDI "dbase2"		// identificator of OPC client

unilog *logg1=NULL;				// new structure of unilog
//--------------------------------------------------------------------------------
// Constructor initializes the string chr_ds_name with the data source name.
dbaccess::dbaccess()
{
 //_mbscpy(chr_ds_name,(UCHAR *) "dk");
}
//--------------------------------------------------------------------------------
VOID dbaccess::dblogon()
{
 logg1 = unilog_Create(ECL_SIDI,absPath(LOG_FNAME), NULL, 0, ll_DEBUG); 
 UL_INFO((LOGID1, "Start"));				// write in log
}
VOID dbaccess::dblogoff()
{
 UL_INFO((LOGID1, "Finish"));
 unilog_Delete(logg1); logg1 = NULL;		// + logs was not currently 
}
//--------------------------------------------------------------------------------
// Allocate environment and connection handle, connect to data source, allocate statement handle.
//--------------------------------------------------------------------------------
BOOL dbaccess::sqlconn(UCHAR * tbname,UCHAR * login,UCHAR * pass)
{
   //_mbscpy(chr_ds_name,(UCHAR *) tbname);
   SQLAllocEnv(&henv);
   SQLAllocConnect(henv,&hdbc);
   rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
   UL_INFO((LOGID1, "SQLAllocHandle(%d)",rc));
   if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) { SQLFreeHandle (SQL_HANDLE_ENV, henv); return FALSE; }   
   rc = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
   UL_INFO((LOGID1, "SQLSetEnvAttr(%d)",rc));
   if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) { SQLFreeHandle (SQL_HANDLE_ENV, henv); return FALSE; }
   rc = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
   UL_INFO((LOGID1, "SQLAllocHandle(%d)",rc));
   if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) { SQLFreeHandle(SQL_HANDLE_DBC, hdbc); return FALSE; }
   rc = SQLConnect(hdbc,(SQLCHAR*) tbname,SQL_NTS,(SQLCHAR*) login,SQL_NTS,(SQLCHAR*) pass,SQL_NTS);
   printf ("sqlconn :: SQLConnect(hdbc,%s,SQL_NTS,(SQLCHAR*) %s,SQL_NTS,(SQLCHAR*) %s,SQL_NTS) = rc",tbname,login,pass,rc);
   if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) { UCHAR State[10]; SQLGetDiagRec (SQL_HANDLE_DBC,hdbc,1,State,NULL,NULL,0,0); printf ("SQLGetDiagRec(%s)",State); UL_INFO((LOGID1, "SQLGetDiagRec(%s)",State)); SQLFreeHandle (SQL_HANDLE_ENV, henv); SQLFreeHandle(SQL_HANDLE_DBC, hdbc); return FALSE; }
   rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
   if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) { SQLDisconnect(hdbc); SQLFreeHandle (SQL_HANDLE_ENV, henv); SQLFreeHandle(SQL_HANDLE_DBC, hdbc); return FALSE; }
   rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmnt);
   if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) { SQLDisconnect(hdbc); SQLFreeHandle (SQL_HANDLE_ENV, henv); SQLFreeHandle(SQL_HANDLE_DBC, hdbc); return FALSE; }
   
   if (!MYSQLSUCCESS(rc))
   {
	  UCHAR State[10];
	  SQLGetDiagRec (SQL_HANDLE_DBC,hdbc,1,State,NULL,NULL,0,0);
	  UL_INFO((LOGID1, "SQLGetDiagRec(%s)",State));
	  //UL_INFO((LOGID1, "SQLFreeStmt(hstmt,SQL_DROP)"));
	  SQLFreeStmt(hstmt,SQL_DROP);
	  SQLFreeStmt(hstmnt,SQL_DROP);
	  //UL_INFO((LOGID1, "SQLDisconnect(hdbc)"));
	  SQLDisconnect(hdbc);
	  //UL_INFO((LOGID1, "sqlconn :: SQLFreeEnv(henv)"));
      SQLFreeEnv(henv);	  
	  //UL_INFO((LOGID1, "sqlconn :: SQLFreeConnect(hdbc)"));
      SQLFreeConnect(hdbc);	  
      //error_out();
      return FALSE;
   }
   rc=SQLAllocStmt(hdbc,&hstmt);
   return TRUE;
}
//--------------------------------------------------------------------------------
// Execute SQL command with SQLExecDirect() ODBC API.
UINT dbaccess::sqlexec(UCHAR * cmdstr, CHAR* data)
{
 SQLRETURN    retcode;	 UINT pos=1;
 SQLUINTEGER szData; 
 //SQLCHAR SqlState[6], Msg[SQL_MAX_MESSAGE_LENGTH]; SQLINTEGER    NativeError;  SQLSMALLINT   MsgLen;
 retcode=SQLExecDirect(hstmt,cmdstr,SQL_NTS);
 //SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, SqlState, &NativeError, Msg, sizeof(Msg), &MsgLen);
 //UL_INFO((LOGID1, "SQLDirect (%s) retcode = %d (%s)",cmdstr,retcode,SqlState)); 
 UL_INFO((LOGID1, "SQLDirect (%s) retcode = %d",cmdstr,retcode)); 

 if (retcode == SQL_SUCCESS) 
	{
	 if (strstr((CHAR*)cmdstr,"SELECT"))
		{	
		 while (TRUE)
			{
			 retcode = SQLFetch(hstmt);
			 //UL_INFO((LOGID1, "sqlexec :: SQLFetch () retcode = %d",retcode));
			 //if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) { error_out(); }
			 if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
				 SQLGetData(hstmt, 1, SQL_C_ULONG, &szData, 0, NULL);
				 //UL_INFO((LOGID1, "[%d]=%d",pos,szData));
				 sprintf (data,"%d",szData);
				 pos++;
				} 
			 else 
				{
				 return pos;
				}
			}	 
		}
 	 if (strstr((CHAR*)cmdstr,"UPDATE"))
		{	
		 while (TRUE)
			{
			 retcode = SQLFetch(hstmt);
			 if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
				 SQLGetData(hstmt, 1, SQL_C_ULONG, &szData, 0, NULL);
				 sprintf (data,"%d",szData);
				 UL_INFO((LOGID1, "[>=%s",data));
				 return 1;
				} 
			 else return 0;
			}	 
		}

	 return 1;
	}
 else 
	{
	 SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, SqlState, &NativeError, Msg, sizeof(Msg), &MsgLen);
	 UL_INFO((LOGID1, "SqlState = %s [%s] %d\n",SqlState,Msg,NativeError));
	}
 return 0;
}
//--------------------------------------------------------------------------------
// Free the statement handle, disconnect, free the connection handle, and
// free the environment handle.
void dbaccess::sqldisconn(void)
{
   SQLRETURN    retcode;
   retcode=SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
   retcode=SQLFreeHandle (SQL_HANDLE_STMT, hstmnt);
   UL_INFO((LOGID1, "sqldisconn :: SQLFreeStmt %d",retcode));
   retcode=SQLDisconnect(hdbc);
   UL_INFO((LOGID1, "sqldisconn :: SQLDisconnect %d",retcode));
   retcode=SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
   UL_INFO((LOGID1, "sqldisconn :: SQLFreeHandle %d",retcode));   
   retcode=SQLFreeHandle(SQL_HANDLE_ENV, henv);
   UL_INFO((LOGID1, "sqldisconn :: SQLFreeHandle %d",retcode));   
}
//--------------------------------------------------------------------------------
