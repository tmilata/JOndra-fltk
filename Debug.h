// Debug.h: interface for the CDebug class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DEBUG_H__49AAB562_F9E2_4B50_8E42_98822DE47606__INCLUDED_)
#define AFX_DEBUG_H__49AAB562_F9E2_4B50_8E42_98822DE47606__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define SAM_VER "1.1"

class CDebug  
{
public:
	CDebug();
	virtual ~CDebug();
	static char strSystem[128];
	static void debug(const char *text, ...);
	void dump(char *str, unsigned char *buf, unsigned long len);
	static int  CreateDir(char* pstrPath);

};

#endif // !defined(AFX_DEBUG_H__49AAB562_F9E2_4B50_8E42_98822DE47606__INCLUDED_)
