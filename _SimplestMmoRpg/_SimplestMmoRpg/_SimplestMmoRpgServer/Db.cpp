#include "Db.h"
#include <iostream>

DB* DB::self = nullptr;

DB::DB() {
}

bool DB::Init() {
	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				// Connect to data source  
				retcode = SQLConnect(hdbc, (SQLWCHAR*)DATABASE_TABLE, SQL_NTS, nullptr, 0, nullptr, 0);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					printf("ODBC Connect OK \n");
				} else {
					printf("ODBC Connect Fail \n");
					ShowError();
					return false;
				}

			} else {
				printf("ODBC SQLAllocHandle(SQL_HANDLE_DBC) Fail \n");
				ShowError();
				return false;
			}
		} else {
			printf("ODBC SQLSetEnvAttr(SQL_OV_ODBC3) Fail \n");
			ShowError();
			return false;
		}
	}
	return true;
}

DB::~DB() {
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
	SQLDisconnect(hdbc);
}

void DB::LoginQuery(const std::wstring& id, std::wstring& name, int& hp, int& level, int& exp, int& x, int& y) {
	auto db = Get();
	if(db){
		db->LoginQueryInternal(id, name, hp, level, exp, x, y);
	}
}

void DB::UpdateStat(const std::wstring& id, int hp, int level, int exp, int x, int y) {
	auto db = Get();
	if (db) {
		db->UpdateStatInternal(id, hp, level, exp, x, y);
	}
}

void DB::LoginQueryInternal(const std::wstring& id, std::wstring& name, int& hp, int& level, int& exp, int& x, int& y) {
	SQLLEN sqlLen = 0;
	SQLHSTMT hstmt = nullptr;
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	// 이미 유저가 가입돼있는지 확인한다
	std::wstringstream ss;
	ss << L"SELECT user_name,user_hp,user_level,user_exp,user_x,user_y FROM user_data WHERE user_id=\'" << id <<
		L"\'";
	retcode = SQLExecDirect(hstmt, const_cast<SQLWCHAR*>(ss.str().c_str()), SQL_NTS);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO){
		retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, &dUser_name, MAX_ID_LEN, &sqlLen);
		retcode = SQLBindCol(hstmt, 2, SQL_INTEGER, &dUser_hp, 100, &sqlLen);
		retcode = SQLBindCol(hstmt, 3, SQL_INTEGER, &dUser_level, 100, &sqlLen);
		retcode = SQLBindCol(hstmt, 4, SQL_INTEGER, &dUser_exp, 100, &sqlLen);
		retcode = SQLBindCol(hstmt, 5, SQL_INTEGER, &dUser_x, 100, &sqlLen);
		retcode = SQLBindCol(hstmt, 6, SQL_INTEGER, &dUser_y, 100, &sqlLen);

		retcode = SQLFetch(hstmt);
		SQLCancel(hstmt);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

		if (retcode == SQL_ERROR){
			ShowError();
		}
		else if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO){
			name = dUser_name;
			hp = dUser_hp;
			level = dUser_level;
			exp = dUser_exp;
			x = dUser_x;
			y = dUser_y;
#ifdef DBLOG
			std::wcout << L"로그인: " << id << L" " << name << L" " << hp << L" " << level << L" " << exp << L" " << x
				<< L" " << y << L"\n";
#endif
		}
		else if (retcode == SQL_NO_DATA){
#ifdef DBLOG
			std::wcout << L"회원가입: ";
#endif
			// 가입이 안돼있으면 테이블에 추가
			ss.str(L"");
			ss << L"INSERT INTO user_data (user_id,user_name,user_hp,user_level,user_exp,user_x,user_y) VALUES(\'"
				<< id << L"\',\'" << name << L"\'," << hp << L"," << level << L"," << exp << L"," << x << L"," << y
				<< L")";
#ifdef DBLOG
			std::wcout << ss.str() << std::endl;
#endif
			retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
			retcode = SQLExecDirect(hstmt, const_cast<SQLWCHAR*>(ss.str().c_str()), SQL_NTS);
			SQLCancel(hstmt);
			SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
			if (retcode == SQL_ERROR){
				ShowError();
			}
			else if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO){
#ifdef DBLOG
				std::wcout << L"회원가입 성공\n";
#endif
			}
			else{
				HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
			}
		}
	}
}

void DB::UpdateStatInternal(const std::wstring& id, int hp, int level, int exp, int x, int y) {
	SQLHSTMT hstmt = nullptr;
	SQLLEN sqlLen = 0;
	std::wstringstream ss;
	ss << L"UPDATE user_data SET user_hp=" << hp << L",user_level=" << level << L",user_exp=" << exp << L",user_x="
		<< x << L",user_y=" << y << L" WHERE user_id='" << id << "'";
#ifdef DBLOG
	std::wcout << ss.str() << std::endl;
#endif
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	retcode = SQLExecDirect(hstmt, const_cast<SQLWCHAR*>(ss.str().c_str()), SQL_NTS);
	SQLCancel(hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO){
#ifdef DBLOG
		std::wcout << L"갱신 성공\n";
#endif
	}
	else{
		ShowError();
	}
}

DB* DB::Create() {
	self = new DB;
	if (!self->Init()) {
		self = nullptr;
	}
	return self;
}

DB* DB::Get() {
	return self;
}

void DB::ShowError() {
#ifdef DBLOG
	printf("DB Error: "); // Insert할때 이미 데이터가 있어도 애러로 나온다
	HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
	printf("\n");
#endif
}

void DB::HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode) {
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE + 1];

	if (RetCode == SQL_INVALID_HANDLE){
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}

	int err;
	while ((err = SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
	                            static_cast<SQLSMALLINT>(sizeof(wszMessage) / sizeof(WCHAR)),
	                            nullptr)) == SQL_SUCCESS){
		// Hide data truncated.. 
		if (wcsncmp(wszState, L"01004", 5)){
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
		else{
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}
