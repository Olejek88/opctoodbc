#define MAX_DATA 100

static SQLCHAR SqlState[6], Msg[SQL_MAX_MESSAGE_LENGTH];
static SQLINTEGER    NativeError;  
static SQLSMALLINT   MsgLen;		 

extern CHAR *absPath(CHAR *fileName);		// return abs path of file

class dbaccess
{
   RETCODE rc;        // ODBC return code
   HENV henv;         // Environment      
   UCHAR szData[MAX_DATA];   // Returned data storage
   SDWORD cbData;			// Output length of data
   UCHAR chr_ds_name[32];   // Data source name

public:
   HDBC hdbc;         // Connection handle   
   HSTMT hstmt;       // Statement handle
   HSTMT hstmnt;       // Statement handle
   dbaccess();           // Constructor
   BOOL sqlconn(UCHAR *,UCHAR *,UCHAR *); // Allocate env, stat, and conn
   UINT sqlexec(UCHAR *, CHAR* data);   // Execute SQL statement
   VOID sqldisconn();   // Free pointers to env, stat, conn,
                        // and disconnect
   VOID error_out();    // Displays errors
   VOID dblogon();		// 
   VOID dblogoff();		// 
};
//------------------------------------------------------------------------------

