// Plugin.cpp : Implementation of CPlugin

#include "stdafx.h"
#include "Plugin.h"

static const LPCWSTR VIRUSTOTAL_CHECK	= L"&VirusTotal Check";
static const LPCWSTR VIRUSTOTAL_HOME	= L"http://www.virustotal.com";
static const LPCWSTR VIRUSTOTAL_URL		= L"http://www.virustotal.com/vt/en/consultamd5";

void CPlugin::InsertCommand(ISMenu* pWebMenu, int nPos, UINT nID, LPCWSTR szItem)
{
	LONG nCount;
	if ( SUCCEEDED( pWebMenu->get_Count( &nCount ) ) )
	{
		for ( int i = 0; i < (int)nCount; ++i )
		{
			CComPtr< ISMenu > pItem;
			LONG nItemID;	// note: -1 - submenu, 0 - separator
			if ( SUCCEEDED( pWebMenu->get_Item( CComVariant( i ), &pItem ) ) && pItem &&
				 SUCCEEDED( pItem->get_CommandID( &nItemID ) ) && (UINT)nItemID == nID )
				// Already in place
				return;
		}
	}

	// Insert new
	pWebMenu->InsertCommand( nPos, nID, CComBSTR( szItem ), NULL );
}

void CPlugin::Request(LPCWSTR szHash)
{
	CComPtr< IWebBrowserApp > pWebBrowserApp;
	HRESULT hr = pWebBrowserApp.CoCreateInstance( CLSID_InternetExplorer );
	if ( SUCCEEDED( hr ) )
	{
		CComSafeArray< BYTE > pPost;
		pPost.Create();
		pPost.Add( 5, (LPBYTE)"hash=" );
		pPost.Add( lstrlenW( szHash ), (LPBYTE)(LPCSTR)CW2A( szHash ) );
		CComBSTR bstrURL( VIRUSTOTAL_URL );
		CComVariant vFlags( 0 );
		CComVariant vFrame( CComBSTR( L"" ) );
		VARIANT vPost;
		VariantInit( &vPost );
		vPost.vt = VT_ARRAY | VT_UI1;
		vPost.parray = pPost;
		CComVariant vHeaders( CComBSTR( L"Content-Type: application/x-www-form-urlencoded\r\n") );
		pWebBrowserApp->Navigate( bstrURL, &vFlags, &vFrame, &vPost, &vHeaders );
	}
}

// IGeneralPlugin

STDMETHODIMP CPlugin::SetApplication( 
	/* [in] */ IApplication __RPC_FAR *pApplication)
{
	if ( ! pApplication )
		return E_POINTER;

	m_pApplication = pApplication;
	return m_pApplication->get_UserInterface( &m_pUserInterface );
}

STDMETHODIMP CPlugin::QueryCapabilities(
	/* [in] */ DWORD __RPC_FAR *pnCaps)
{
	if ( ! pnCaps )
		return E_POINTER;

	return S_OK;
}

STDMETHODIMP CPlugin::Configure()
{
	ShellExecute( NULL, NULL, VIRUSTOTAL_HOME, NULL, NULL, SW_SHOWDEFAULT );

	return S_OK;
}

STDMETHODIMP CPlugin::OnSkinChanged()
{
	// Recreate lost menu items
	return InsertCommands();
}

// ICommandPlugin

STDMETHODIMP CPlugin::RegisterCommands()
{
	if ( ! m_pUserInterface )
		return E_UNEXPECTED;

	HRESULT hr = m_pUserInterface->RegisterCommand( CComBSTR( L"VirusTotalPlugin_Check" ),
		LoadIcon( _AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE( IDI_ICON ) ),
		&m_nCmdCheck );
	if ( SUCCEEDED( hr ) )
	{
		return S_OK;
	}

	return E_FAIL;
}

STDMETHODIMP CPlugin::InsertCommands()
{
	if ( ! m_pUserInterface )
		return E_UNEXPECTED;

	CComPtr< ISMenu > pSearchMenu;
	if ( SUCCEEDED( m_pUserInterface->GetMenu( CComBSTR( L"CSearchWnd" ),
		VARIANT_FALSE, &pSearchMenu ) ) && pSearchMenu )
	{
		CComPtr< ISMenu > pWebMenu;
		if ( SUCCEEDED( pSearchMenu->get_Item( CComVariant( 9 ), &pWebMenu ) ) && pWebMenu )
		{
			InsertCommand( pWebMenu, 1, m_nCmdCheck, VIRUSTOTAL_CHECK );
		}
	}

	CComPtr< ISMenu > pFileMenu;
	if ( SUCCEEDED( m_pUserInterface->GetMenu( CComBSTR( L"CLibraryFileView.Physical" ),
		VARIANT_FALSE, &pFileMenu ) ) && pFileMenu )
	{
		CComPtr< ISMenu > pWebMenu;
		if ( SUCCEEDED( pFileMenu->get_Item( CComVariant( 9 ), &pWebMenu ) ) && pWebMenu )
		{
			InsertCommand( pWebMenu, 2, m_nCmdCheck, VIRUSTOTAL_CHECK );
		}
	}

	CComPtr< ISMenu > pListMenu;
	if ( SUCCEEDED( m_pUserInterface->GetMenu( CComBSTR( L"WebServices.List.Menu" ),
		VARIANT_FALSE, &pListMenu ) ) && pListMenu )
	{
		InsertCommand( pListMenu, 1, m_nCmdCheck, VIRUSTOTAL_CHECK );
	}

	return E_FAIL;
}

STDMETHODIMP CPlugin::OnUpdate( 
    /* [in] */ UINT nCommandID,
    /* [out][in] */ TRISTATE __RPC_FAR *pbVisible,
    /* [out][in] */ TRISTATE __RPC_FAR *pbEnabled,
    /* [out][in] */ TRISTATE __RPC_FAR *pbChecked)
{
	if ( ! pbVisible || ! pbEnabled || ! pbChecked )
		return E_POINTER;

	if ( ! m_pUserInterface )
		return E_UNEXPECTED;

	if ( nCommandID == m_nCmdCheck )
	{
		*pbEnabled = TRI_FALSE;
		*pbVisible = TRI_TRUE;
		*pbChecked = TRI_UNKNOWN;

		CComPtr< IGenericView > pGenericView;
		HRESULT hr = m_pUserInterface->get_ActiveView( &pGenericView );
		if ( SUCCEEDED( hr ) && pGenericView )
		{
			LONG nCount = 0;
			hr = pGenericView->get_Count( &nCount );
			if ( SUCCEEDED( hr ) && nCount )
			{
				*pbEnabled = TRI_TRUE;
			}
		}
		return S_OK;
	}

	return S_FALSE;
}

STDMETHODIMP CPlugin::OnCommand( 
	/* [in] */ UINT nCommandID)
{
	if ( ! m_pUserInterface )
		return E_UNEXPECTED;

	if ( nCommandID == m_nCmdCheck )
	{
		CComPtr< IGenericView > pGenericView;
		HRESULT hr = m_pUserInterface->get_ActiveView( &pGenericView );
		if ( SUCCEEDED( hr ) && pGenericView )
		{
			LONG nCount = 0;
			hr = pGenericView->get_Count( &nCount );
			if ( SUCCEEDED( hr ) && nCount )
			{
				for ( LONG i = 0; i < nCount; ++i )
				{
					CComVariant pItem;
					hr = pGenericView->get_Item( CComVariant( i ), &pItem );
					if ( FAILED( hr ) )
						break;

					CComBSTR pSHA1;
					CComBSTR pMD5;
					if ( pItem.vt == VT_I4 )
					{
						CComPtr< ILibrary > pLibrary;
						hr = m_pApplication->get_Library( &pLibrary );
						if ( SUCCEEDED( hr ) && pLibrary )
						{
							CComPtr< ILibraryFile > pLibraryFile;
							hr = pLibrary->FindByIndex( pItem.lVal, &pLibraryFile );
							if ( SUCCEEDED( hr ) && pLibraryFile )
							{
								pLibraryFile->get_Hash( URN_MD5,  ENCODING_BASE16, &pMD5 );
								pLibraryFile->get_Hash( URN_SHA1, ENCODING_BASE16, &pSHA1 );
							}
						}
					}
					else if ( pItem.vt == VT_DISPATCH )
					{
						CComQIPtr< IShareazaFile > pShareazaFile( pItem.pdispVal );
						if ( pShareazaFile )
						{
							pShareazaFile->get_Hash( URN_MD5,  ENCODING_BASE16, &pMD5 );
							pShareazaFile->get_Hash( URN_SHA1, ENCODING_BASE16, &pSHA1 );
						}
					}

					if ( pMD5.Length() )
					{
						Request( pMD5 );
					}
					else if ( pSHA1.Length() )
					{
						Request( pSHA1 );
					}
				}
				return S_OK;
			}
		}
	}

	return S_FALSE;
}
