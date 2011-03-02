//
// DlgURLCopy.cpp
//
// Copyright (c) Shareaza Development Team, 2002-2011.
// This file is part of SHAREAZA (shareaza.sourceforge.net)
//
// Shareaza is free software; you can redistribute it
// and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Shareaza is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Shareaza; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "ShareazaFile.h"
#include "CoolInterface.h"
#include "DlgURLCopy.h"
#include "Download.h"
#include "Downloads.h"
#include "Network.h"
#include "Transfer.h"
#include "Transfers.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CURLCopyDlg, CSkinDialog)

BEGIN_MESSAGE_MAP(CURLCopyDlg, CSkinDialog)
	ON_WM_CTLCOLOR()
	ON_WM_SETCURSOR()
	ON_BN_CLICKED(IDC_INCLUDE_SELF, &CURLCopyDlg::OnIncludeSelf)
	ON_STN_CLICKED(IDC_URL_HOST, &CURLCopyDlg::OnStnClickedUrlHost)
	ON_STN_CLICKED(IDC_URL_MAGNET, &CURLCopyDlg::OnStnClickedUrlMagnet)
	ON_STN_CLICKED(IDC_URL_ED2K, &CURLCopyDlg::OnStnClickedUrlEd2k)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CURLCopyDlg dialog

CURLCopyDlg::CURLCopyDlg(CWnd* pParent) : CSkinDialog(CURLCopyDlg::IDD, pParent)
{
}

void CURLCopyDlg::DoDataExchange(CDataExchange* pDX)
{
	CSkinDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_INCLUDE_SELF, m_wndIncludeSelf);
	DDX_Control(pDX, IDC_MESSAGE, m_wndMessage);
	DDX_Text(pDX, IDC_URL_HOST, m_sHost);
	DDX_Text(pDX, IDC_URL_MAGNET, m_sMagnet);
	DDX_Text(pDX, IDC_URL_ED2K, m_sED2K);
}

void CURLCopyDlg::Add(const CShareazaFile* pFile)
{
	ASSERT( pFile != NULL );
	m_pFile = pFile;
}

/////////////////////////////////////////////////////////////////////////////
// CURLCopyDlg message handlers

BOOL CURLCopyDlg::OnInitDialog()
{
	CSkinDialog::OnInitDialog();

	SkinMe( NULL, IDI_WEB_URL );
	
	m_wndIncludeSelf.ShowWindow( ( Network.IsListening() && m_pFile->m_sURL.IsEmpty() )
		? SW_SHOW : SW_HIDE );

	OnIncludeSelf();

	return TRUE;
}

void CURLCopyDlg::OnIncludeSelf()
{
	UpdateData();

	BOOL bIncludeSelf = m_wndIncludeSelf.GetCheck();

	CString strURN, strIncludeSelfURN, strTemp;

	if ( m_pFile->m_oTiger && m_pFile->m_oSHA1 )
	{
		strURN	= _T("xt=urn:bitprint:")
				+ m_pFile->m_oSHA1.toString() + '.'
				+ m_pFile->m_oTiger.toString();
	}
	else if ( m_pFile->m_oSHA1 )
	{
		strURN = _T("xt=") + m_pFile->m_oSHA1.toUrn();
	}
	else if ( m_pFile->m_oTiger )
	{
		strURN = _T("xt=") + m_pFile->m_oTiger.toUrn();
	}

	if ( m_pFile->m_oED2K )
	{
		strTemp = _T("xt=") + m_pFile->m_oED2K.toUrn();
		if ( strURN.GetLength() ) strURN += _T("&");
		strURN += strTemp;
	}

	if ( m_pFile->m_oMD5 && ! m_pFile->m_oTiger && ! m_pFile->m_oSHA1 && ! m_pFile->m_oED2K )
	{
		strTemp = _T("xt=") + m_pFile->m_oMD5.toUrn();
		if ( strURN.GetLength() ) strURN += _T("&");
		strURN += strTemp;
	}

	if ( m_pFile->m_oBTH && ! m_pFile->m_oTiger && ! m_pFile->m_oSHA1 && ! m_pFile->m_oED2K && ! m_pFile->m_oMD5 )
	{
		strTemp = _T("xt=") + m_pFile->m_oBTH.toUrn();
		if ( strURN.GetLength() ) strURN += _T("&");
		strURN += strTemp;
		
		CSingleLock oLock( &Transfers.m_pSection, TRUE );
		if ( CDownload* pDownload = Downloads.FindByBTH( m_pFile->m_oBTH ) )
		{
			if ( pDownload->IsTorrent() )
			{
				CString sTracker = URLEncode( pDownload->m_pTorrent.GetTrackerAddress() );
				if ( sTracker.GetLength() )
				{
					strURN += _T("&tr=") + sTracker;
				}
			}
		}
	}

	m_sMagnet = strURN;

	if ( m_pFile->m_nSize != 0 && m_pFile->m_nSize != SIZE_UNKNOWN )
	{
		CString strSize;

		strSize.Format( _T("xl=%I64i"),
			m_pFile->m_nSize );

		if ( m_sMagnet.GetLength() ) m_sMagnet += _T("&");
		m_sMagnet += strSize;
	}

	if ( m_pFile->m_sName.GetLength() )
	{
		CString strName = URLEncode( m_pFile->m_sName );

		if ( m_sMagnet.GetLength() ) m_sMagnet += _T("&");
		if ( strURN.GetLength() )
			m_sMagnet += _T("dn=") + strName;
		else
			m_sMagnet += _T("kt=") + strName;
	}

	m_sMagnet = _T("magnet:?") + m_sMagnet;

	if ( bIncludeSelf )
	{
		CString strURL = m_pFile->GetURL( Network.m_pHost.sin_addr,
			htons( Network.m_pHost.sin_port ) );
		if ( strURL.GetLength() )
			m_sMagnet += _T("&xs=") + URLEncode( strURL );
	}

	if ( m_pFile->m_oED2K &&
		( m_pFile->m_nSize != 0 && m_pFile->m_nSize != SIZE_UNKNOWN ) &&
		m_pFile->m_sName.GetLength() )
	{
		m_sED2K.Format( _T("ed2k://|file|%s|%I64i|%s|/"),
			(LPCTSTR)URLEncode( m_pFile->m_sName ),
			m_pFile->m_nSize,
			(LPCTSTR)m_pFile->m_oED2K.toString() );

		if ( bIncludeSelf )
		{
			CString strURL2;

			strURL2.Format ( _T("%s:%i"),
					(LPCTSTR)CString( inet_ntoa( Network.m_pHost.sin_addr ) ),
					htons( Network.m_pHost.sin_port ) );

			m_sED2K += _T("|sources,")
					+ strURL2
					+ _T("|/");
		}
	}

	if ( m_pFile->m_sURL.GetLength() )
	{
		m_sHost = m_pFile->m_sURL;
	}
	else if ( bIncludeSelf )
	{
		m_sHost = m_pFile->GetURL( Network.m_pHost.sin_addr,
			htons( Network.m_pHost.sin_port ) );
	}
	else
	{
		m_sHost.Empty();
	}

	UpdateData( FALSE );
}

HBRUSH CURLCopyDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CSkinDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	if ( pWnd && pWnd != &m_wndMessage )
	{
		TCHAR szName[32];
		GetClassName( pWnd->GetSafeHwnd(), szName, 32 );

		if ( ! _tcsicmp( szName, _T("Static") ) )
		{
			pDC->SetTextColor( CoolInterface.m_crTextLink );
			pDC->SelectObject( &theApp.m_gdiFontLine );
		}
	}

	return hbr;
}

BOOL CURLCopyDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	CPoint point;
	GetCursorPos( &point );

	for ( pWnd = GetWindow( GW_CHILD ) ; pWnd ; pWnd = pWnd->GetNextWindow() )
	{
		TCHAR szName[32];
		GetClassName( pWnd->GetSafeHwnd(), szName, 32 );

		if ( ! _tcsicmp( szName, _T("Static") ) && pWnd != &m_wndMessage )
		{
			CString strText;
			CRect rc;

			pWnd->GetWindowRect( &rc );

			if ( rc.PtInRect( point ) )
			{
				pWnd->GetWindowText( strText );

				if ( strText.GetLength() )
				{
					SetCursor( theApp.LoadCursor( IDC_HAND ) );
					return TRUE;
				}
			}
		}
	}

	return CSkinDialog::OnSetCursor( pWnd, nHitTest, message );
}

BOOL CURLCopyDlg::SetClipboardText(CString& strText)
{
	if ( ! AfxGetMainWnd()->OpenClipboard() ) return FALSE;

	EmptyClipboard();

	CT2CW pszWide( (LPCTSTR)strText );
	HANDLE hMem = GlobalAlloc( GMEM_MOVEABLE|GMEM_DDESHARE, ( wcslen(pszWide) + 1 ) * sizeof(WCHAR) );
	if ( hMem )
	{
		LPVOID pMem = GlobalLock( hMem );
		if ( pMem )
		{
			CopyMemory( pMem, pszWide, ( wcslen(pszWide) + 1 ) * sizeof(WCHAR) );
			GlobalUnlock( hMem );
			SetClipboardData( CF_UNICODETEXT, hMem );
		}
	}

	CloseClipboard();

	return TRUE;
}

void CURLCopyDlg::OnStnClickedUrlHost()
{
	UpdateData();

	if ( m_sHost.GetLength() )
	{
		SetClipboardText( m_sHost );

		CSkinDialog::OnOK();
	}
}

void CURLCopyDlg::OnStnClickedUrlMagnet()
{
	UpdateData();

	if ( m_sMagnet.GetLength() )
	{
		SetClipboardText( m_sMagnet );

		CSkinDialog::OnOK();
	}
}

void CURLCopyDlg::OnStnClickedUrlEd2k()
{
	UpdateData();

	if ( m_sED2K.GetLength() )
	{
		SetClipboardText( m_sED2K );

		CSkinDialog::OnOK();
	}
}
