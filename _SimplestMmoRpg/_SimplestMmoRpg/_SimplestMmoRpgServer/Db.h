#pragma once
#include <windows.h>
#define UNICODE  
#include <iosfwd>
#include <sqlext.h>
#include <sstream>

#include "protocol.h"

constexpr const WCHAR* DATABASE_TABLE = L"game_db_odbc";

class DB {
	SQLHENV henv{};
	SQLHDBC hdbc{};
	SQLHSTMT hstmt = nullptr;
	SQLRETURN retcode;
	SQLWCHAR dUser_name[MAX_ID_LEN]{}, dUser_id[MAX_ID_LEN]{};
	SQLINTEGER dUser_level{}, dUser_hp{}, dUser_exp{}, dUser_x{}, dUser_y{};
	std::wstringstream ss;
public:
	DB();
	~DB();

	void LoginQuery(const std::wstring& id, std::wstring& name, int& hp, int& level, int& exp, int& x, int& y);

	void UpdateStat(const std::wstring& id, int hp, int level, int exp, int x, int y);

private:
	void ShowError();

	void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);
};
