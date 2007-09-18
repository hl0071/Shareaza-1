//
// DlgFilterSearch.cpp
//
// Copyright (c) Shareaza Development Team, 2002-2007.
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
#include "DlgFilterSearch.h"
#include "MatchObjects.h"
#include "ResultFilters.h"
#include "DlgSaveFilterAs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CFilterSearchDlg, CSkinDialog)
	//{{AFX_MSG_MAP(CFilterSearchDlg)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_SAVE_FILTER, OnBnClickedSaveFilter)
	ON_WM_DESTROY()
	ON_CBN_SELCHANGE(IDC_FILTERS, OnCbnSelChangeFilters)
	ON_BN_CLICKED(IDC_DELETE_FILTER, OnBnClickedDeleteFilter)
	ON_BN_CLICKED(IDC_SET_DEFAULT_FILTER, OnBnClickedSetDefaultFilter)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CFilterSearchDlg dialog

CFilterSearchDlg::CFilterSearchDlg(CWnd* pParent, CMatchList* pMatches) : CSkinDialog(CFilterSearchDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CFilterSearchDlg)
	m_sFilter = _T("");
	m_bHideBusy = FALSE;
	m_bHideLocal = FALSE;
	m_bHidePush = FALSE;
	m_bHideReject = FALSE;
	m_bHideUnstable = FALSE;
	m_bHideBogus = FALSE;
	m_bHideDRM= FALSE;
	m_bHideAdult= FALSE;
	m_bHideSuspicious= FALSE;
	m_nSources = 0;
	m_sMaxSize = _T("");
	m_sMinSize = _T("");
	m_bDefault = FALSE;
	//}}AFX_DATA_INIT
	m_pMatches = pMatches;
}

void CFilterSearchDlg::DoDataExchange(CDataExchange* pDX)
{
	CSkinDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFilterSearchDlg)
	DDX_Control(pDX, IDC_SOURCES_SPIN, m_wndSources);
	DDX_Text(pDX, IDC_FILTER, m_sFilter);
	DDX_Check(pDX, IDC_FILTER_BUSY, m_bHideBusy);
	DDX_Check(pDX, IDC_FILTER_LOCAL, m_bHideLocal);
	DDX_Check(pDX, IDC_FILTER_PUSH, m_bHidePush);
	DDX_Check(pDX, IDC_FILTER_REJECT, m_bHideReject);
	DDX_Check(pDX, IDC_FILTER_UNSTABLE, m_bHideUnstable);
	DDX_Check(pDX, IDC_FILTER_BOGUS, m_bHideBogus);
	DDX_Check(pDX, IDC_FILTER_DRM, m_bHideDRM);
	DDX_Check(pDX, IDC_ADULT_FILTER, m_bHideAdult);
	DDX_Check(pDX, IDC_FILTER_SUS, m_bHideSuspicious);
	DDX_Text(pDX, IDC_SOURCES, m_nSources);
	DDX_Text(pDX, IDC_MAX_SIZE, m_sMaxSize);
	DDX_Text(pDX, IDC_MIN_SIZE, m_sMinSize);
	DDX_Check(pDX, IDC_SET_DEFAULT_FILTER, m_bDefault);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_FILTERS, m_Filters);
}

/////////////////////////////////////////////////////////////////////////////
// CFilterSearchDlg message handlers

BOOL CFilterSearchDlg::OnInitDialog()
{
	CSkinDialog::OnInitDialog();

	SkinMe( _T("CFilterSearchDlg"), IDR_SEARCHFRAME );

	if ( m_pMatches != NULL ) 
	{
		m_pResultFilters = m_pMatches->m_pResultFilters;
		m_pResultFilters->Load();
		UpdateList();
		UpdateFields();
	}

	ASSERT( m_pResultFilters != NULL );

	m_wndSources.SetRange( 0, 256 );

	UpdateData( FALSE );

	return TRUE;
}

void CFilterSearchDlg::OnOK()
{
	UpdateData( TRUE );

	if ( m_pMatches != NULL )
	{
		m_pMatches->m_sFilter			= m_sFilter;
		m_pMatches->m_bFilterBusy		= m_bHideBusy;
		m_pMatches->m_bFilterPush		= m_bHidePush;
		m_pMatches->m_bFilterUnstable	= m_bHideUnstable;
		m_pMatches->m_bFilterLocal		= m_bHideLocal;
		m_pMatches->m_bFilterReject		= m_bHideReject;
		m_pMatches->m_bFilterBogus		= m_bHideBogus;
		m_pMatches->m_bFilterDRM		= m_bHideDRM;
		m_pMatches->m_bFilterAdult		= m_bHideAdult;
		m_pMatches->m_bFilterSuspicious	= m_bHideSuspicious;
		m_pMatches->m_nFilterMinSize	= Settings.ParseVolume( m_sMinSize, FALSE );
		m_pMatches->m_nFilterMaxSize	= Settings.ParseVolume( m_sMaxSize, FALSE );
		m_pMatches->m_nFilterSources	= m_nSources;
	}

	CSkinDialog::OnOK();
}


void CFilterSearchDlg::OnBnClickedSaveFilter()
{
	CSaveFilterAsDlg dlg( this );

	if ( dlg.DoModal() == IDOK )
	{
		int nExistingFilter = m_pResultFilters->Search( dlg.m_sName );
		CFilterOptions *pOptions;
		if ( nExistingFilter >= 0 )
		{
			pOptions = m_pResultFilters->m_pFilters[nExistingFilter];
		}
		else
		{
			pOptions = new CFilterOptions;
		}

		UpdateData(TRUE);

		pOptions->m_sName				= dlg.m_sName;
		pOptions->m_sFilter				= m_sFilter;
		pOptions->m_bFilterBusy			= m_bHideBusy;
		pOptions->m_bFilterPush			= m_bHidePush;
		pOptions->m_bFilterUnstable		= m_bHideUnstable;
		pOptions->m_bFilterLocal		= m_bHideLocal;
		pOptions->m_bFilterReject		= m_bHideReject;
		pOptions->m_bFilterBogus		= m_bHideBogus;
		pOptions->m_bFilterDRM			= m_bHideDRM;
		pOptions->m_bFilterAdult		= m_bHideAdult;
		pOptions->m_bFilterSuspicious	= m_bHideSuspicious;
		pOptions->m_nFilterMinSize		= Settings.ParseVolume( m_sMinSize, FALSE );
		pOptions->m_nFilterMaxSize		= Settings.ParseVolume( m_sMaxSize, FALSE );
		pOptions->m_nFilterSources		= m_nSources;

		if ( nExistingFilter < 0 )
		{
			m_pResultFilters->Add( pOptions );
		}
		m_pResultFilters->Save();

		UpdateList();

		m_Filters.SetCurSel( m_pResultFilters->m_nFilters - 1 ); // select the last item added
		OnCbnSelChangeFilters();
	}
}

//Update the filter fields with current data
void CFilterSearchDlg::UpdateFields()
{
	m_sFilter			= m_pMatches->m_sFilter;
	m_bHideBusy			= m_pMatches->m_bFilterBusy;
	m_bHidePush			= m_pMatches->m_bFilterPush;
	m_bHideUnstable		= m_pMatches->m_bFilterUnstable;
	m_bHideLocal		= m_pMatches->m_bFilterLocal;
	m_bHideReject		= m_pMatches->m_bFilterReject;
	m_bHideBogus		= m_pMatches->m_bFilterBogus;
	m_bHideDRM			= m_pMatches->m_bFilterDRM;
	m_bHideAdult		= m_pMatches->m_bFilterAdult;
	m_bHideSuspicious	= m_pMatches->m_bFilterSuspicious;
	m_nSources			= m_pMatches->m_nFilterSources;

	if ( m_pMatches->m_nFilterMinSize > 0 )
		m_sMinSize	= Settings.SmartVolume( m_pMatches->m_nFilterMinSize );
	else
		m_sMinSize.Empty();

	if ( m_pMatches->m_nFilterMaxSize > 0 )
		m_sMaxSize	= Settings.SmartVolume( m_pMatches->m_nFilterMaxSize );
	else
		m_sMaxSize.Empty();

	DWORD sel = m_Filters.GetCurSel();

	if ( sel != CB_ERR )
		m_bDefault = ( sel == m_pResultFilters->m_nDefault );

	UpdateData(FALSE);
}
//update the filter selection combo-box with filter list items
void CFilterSearchDlg::UpdateList()
{
	m_Filters.ResetContent();

	for ( DWORD i = 0; i < m_pResultFilters->m_nFilters; i++ )
	{
		if ( i == m_pResultFilters->m_nDefault )
			m_Filters.AddString( m_pResultFilters->m_pFilters[i]->m_sName + " *" );
		else
			m_Filters.AddString( m_pResultFilters->m_pFilters[i]->m_sName );

//		m_Filters.SetItemDataPtr( i, m_pResultFilters->m_pFilters[i] ); //save a pointer to the item
	}

	GetDlgItem( IDC_SET_DEFAULT_FILTER )->EnableWindow( m_pResultFilters->m_nFilters > 0 );
}

void CFilterSearchDlg::OnCbnSelChangeFilters()
{
	DWORD sel = m_Filters.GetCurSel();

	if ( sel != CB_ERR )
	{
//		CFilterOptions *pOptions = (CFilterOptions *) m_Filters.GetItemDataPtr(sel);
		CFilterOptions *pOptions = m_pResultFilters->m_pFilters[sel];

		m_sFilter			= pOptions->m_sFilter;
		m_bHideBusy			= pOptions->m_bFilterBusy;
		m_bHidePush			= pOptions->m_bFilterPush;
		m_bHideUnstable		= pOptions->m_bFilterUnstable;
		m_bHideLocal		= pOptions->m_bFilterLocal;
		m_bHideReject		= pOptions->m_bFilterReject;
		m_bHideBogus		= pOptions->m_bFilterBogus;
		m_bHideDRM			= pOptions->m_bFilterDRM;
		m_bHideAdult		= pOptions->m_bFilterAdult;
		m_bHideSuspicious	= pOptions->m_bFilterSuspicious;
		m_nSources			= pOptions->m_nFilterSources;

		if ( pOptions->m_nFilterMinSize > 0 )
			m_sMinSize	= Settings.SmartVolume( pOptions->m_nFilterMinSize );
		else
			m_sMinSize.Empty();

		if ( pOptions->m_nFilterMaxSize > 0 )
			m_sMaxSize	= Settings.SmartVolume( pOptions->m_nFilterMaxSize );
		else
			m_sMaxSize.Empty();

		m_bDefault = ( sel == m_pResultFilters->m_nDefault );

		UpdateData(FALSE);
	}
}

void CFilterSearchDlg::OnBnClickedDeleteFilter()
{
	UpdateData(TRUE);

	DWORD sel = m_Filters.GetCurSel();

	if ( sel != CB_ERR )
	{
		CString strMessage;
		LoadString( strMessage, IDS_FILTER_DELETE_CONFIRM );
		if ( AfxMessageBox( strMessage, MB_ICONQUESTION | MB_YESNO ) == IDYES )
		{
			m_pResultFilters->Remove( sel );
			m_pResultFilters->Save();
			UpdateList();
			m_Filters.SetCurSel( min( sel, m_pResultFilters->m_nFilters - 1 ) );
			m_bDefault = FALSE;
			OnCbnSelChangeFilters();
			UpdateData(FALSE);
		}
	}
}

void CFilterSearchDlg::OnBnClickedSetDefaultFilter()
{
	UpdateData( TRUE );

	DWORD sel = m_Filters.GetCurSel();
	if ( m_bDefault )
	{
		if ( sel != CB_ERR )
			m_pResultFilters->m_nDefault = sel;
	}
	else
	{
		m_pResultFilters->m_nDefault = NONE;
	}
	UpdateList();
	m_Filters.SetCurSel( sel );
	m_pResultFilters->Save();
}
