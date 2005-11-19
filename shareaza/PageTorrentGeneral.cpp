//
// PageTorrentGeneral.cpp
//
// Copyright (c) Shareaza Development Team, 2002-2005.
// This file is part of SHAREAZA (www.shareaza.com)
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
#include "Settings.h"

#include "ShellIcons.h"
#include "BTInfo.h"
#include "PageTorrentGeneral.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNCREATE(CTorrentGeneralPage, CTorrentInfoPage)

BEGIN_MESSAGE_MAP(CTorrentGeneralPage, CTorrentInfoPage)
	//{{AFX_MSG_MAP(CTorrentGeneralPage)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTorrentGeneralPage property page

CTorrentGeneralPage::CTorrentGeneralPage() : CTorrentInfoPage( CTorrentGeneralPage::IDD )
{
	//{{AFX_DATA_INIT(CTorrentGeneralPage)
	m_sName = _T("");
	m_sComment = _T("");
	m_sCreationDate = _T("");
	m_sCreatedBy = _T("");
	//}}AFX_DATA_INIT
}

CTorrentGeneralPage::~CTorrentGeneralPage()
{
}

void CTorrentGeneralPage::DoDataExchange(CDataExchange* pDX)
{
	CTorrentInfoPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTorrentGeneralPage)
	DDX_Text(pDX, IDC_TORRENT_NAME, m_sName);
	DDX_Text(pDX, IDC_TORRENT_COMMENTS, m_sComment);
	DDX_Text(pDX, IDC_TORRENT_CREATEDBY, m_sCreatedBy );
	DDX_Text(pDX, IDC_TORRENT_CREATIONDATE, m_sCreationDate );
	DDX_Control(pDX, IDC_TORRENT_STARTDOWNLOADS, m_wndStartDownloads);
	//}}AFX_DATA_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CTorrentGeneralPage message handlers

BOOL CTorrentGeneralPage::OnInitDialog()
{
	CBTInfo *pInfo = GetTorrentInfo();
	CTorrentInfoPage::OnInitDialog();

	m_sName			= pInfo->m_sName;
	m_sComment		= pInfo->m_sComment;
	m_sCreatedBy	= pInfo->m_sCreatedBy;
	if ( pInfo->m_tCreationDate > 0 )
	{
		CTime pTime( (time_t)pInfo->m_tCreationDate );
		m_sCreationDate = pTime.Format( _T("%Y-%m-%d  %H:%M") );
	}

	m_wndStartDownloads.SetItemData( 0, dtAlways );
	m_wndStartDownloads.SetItemData( 1, dtWhenRatio );
	m_wndStartDownloads.SetItemData( 2, dtNever );

	m_wndStartDownloads.SetCurSel( pInfo->m_nStartDownloads );

	UpdateData( FALSE );
	
	return TRUE;
}

void CTorrentGeneralPage::OnOK()
{
	UpdateData();
	CBTInfo *pInfo = GetTorrentInfo();

	// Update the starting of torrent transfers
	pInfo->m_nStartDownloads = m_wndStartDownloads.GetCurSel();
	if ( pInfo->m_nStartDownloads > dtNever ) pInfo->m_nStartDownloads = dtAlways;
	

	CTorrentInfoPage::OnOK();
}


