//
// DownloadTransferED2K.cpp
//
// Copyright (c) Shareaza Development Team, 2002-2004.
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
#include "Download.h"
#include "Downloads.h"
#include "DownloadSource.h"
#include "DownloadTransfer.h"
#include "DownloadTransferED2K.h"
#include "FragmentedFile.h"
#include "Datagrams.h"
#include "EDClients.h"
#include "EDClient.h"
#include "EDPacket.h"
#include "Network.h"
#include "Buffer.h"
#include "ED2K.h"

#include <zlib.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define BUFFER_SIZE		8192


//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K construction

CDownloadTransferED2K::CDownloadTransferED2K(CDownloadSource* pSource) : CDownloadTransfer( pSource, PROTOCOL_ED2K )
{
	m_pClient		= NULL;
	m_bHashset		= FALSE;
	m_tRequest		= 0;
	m_tRanking		= 0;
	ASSERT( m_oPossible.IsEmpty() );
	ASSERT( m_oRequested.IsEmpty() );

	m_bUDP			= FALSE;
	
	m_pInflatePtr		= NULL;
	m_pInflateBuffer	= new CBuffer();
	
	ASSERT( m_pDownload->m_oED2K.IsValid() );
}

CDownloadTransferED2K::~CDownloadTransferED2K()
{
	ClearRequests();
	delete m_pInflateBuffer;
	
	
#ifdef _DEBUG
	ASSERT( m_pClient == NULL );
	
	for ( CEDClient* pClient = EDClients.GetFirst() ; pClient ; pClient = pClient->m_pEdNext )
	{
		ASSERT( pClient->m_pDownload != this );
	}
#endif
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K initiate

BOOL CDownloadTransferED2K::Initiate()
{
	ASSERT( m_pClient == NULL );
	ASSERT( m_nState == dtsNull );
	
	if ( ! m_pDownload->m_oED2K.IsValid() || m_pDownload->m_nSize == SIZE_UNKNOWN )
	{
		Close( TS_FALSE );
		return FALSE;
	}
	
	m_pClient = EDClients.Connect(
		m_pSource->m_pAddress.S_un.S_addr,
		m_pSource->m_nPort,
		m_pSource->m_nServerPort ? &m_pSource->m_pServerAddress : NULL,
		m_pSource->m_nServerPort,
		m_pSource->m_bGUID ? &m_pSource->m_oGUID : NULL );
	
	if ( m_pClient == NULL )
	{
		Close( EDClients.IsFull() ? TS_TRUE : TS_FALSE );
		return FALSE;
	}
	
	SetState( dtsConnecting );
	m_tConnected = GetTickCount();
	
	if ( ! m_pClient->AttachDownload( this ) )
	{
		SetState( dtsNull );
		m_pClient = NULL;
		Close( TS_TRUE );
		return FALSE;
	}
	
	m_pHost			= m_pClient->m_pHost;
	m_sAddress		= m_pClient->m_sAddress;
	
	m_pClient->m_mInput.pLimit = &Downloads.m_nLimitDonkey;
	
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K close

void CDownloadTransferED2K::Close(TRISTATE bKeepSource)
{
	if ( m_pClient != NULL )
	{
		m_pClient->OnDownloadClose();
		m_pClient = NULL;
	}
	
	CDownloadTransfer::Close( bKeepSource );
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K bandwidth control

void CDownloadTransferED2K::Boost()
{
	if ( m_pClient == NULL ) return;
	m_pClient->m_mInput.pLimit = NULL;
}

DWORD CDownloadTransferED2K::GetAverageSpeed()
{
	return m_pSource->m_nSpeed = GetMeasuredSpeed();
}

DWORD CDownloadTransferED2K::GetMeasuredSpeed()
{
	if ( m_pClient == NULL ) return 0;
	m_pClient->Measure();
	return m_pClient->m_mInput.nMeasure;
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K run event

BOOL CDownloadTransferED2K::OnRun()
{
	return OnRunEx( GetTickCount() );
}

BOOL CDownloadTransferED2K::OnRunEx(DWORD tNow)
{
	switch ( m_nState )
	{
	case dtsConnecting:
		if ( tNow > m_tConnected && tNow - m_tConnected > Settings.Connection.TimeoutConnect * 2 )
		{
			theApp.Message( MSG_ERROR, IDS_CONNECTION_TIMEOUT_CONNECT, (LPCTSTR)m_sAddress );
			Close( TS_TRUE );
			return FALSE;
		}
		break;
	case dtsRequesting:
	case dtsEnqueue:
		if ( tNow > m_tRequest && tNow - m_tRequest > Settings.Connection.TimeoutHandshake * 2 )
		{
			theApp.Message( MSG_ERROR, IDS_DOWNLOAD_REQUEST_TIMEOUT, (LPCTSTR)m_sAddress );
			Close( TS_UNKNOWN );
			return FALSE;
		}
		break;
	case dtsQueued:
		return RunQueued( tNow );
	case dtsDownloading:
	case dtsHashset:
		if ( tNow > m_pClient->m_mInput.tLast &&
			 tNow - m_pClient->m_mInput.tLast > Settings.Connection.TimeoutTraffic * 2 )
		{
			theApp.Message( MSG_ERROR, IDS_DOWNLOAD_TRAFFIC_TIMEOUT, (LPCTSTR)m_sAddress );
			Close( TS_TRUE );
			return FALSE;
		}
		break;
	}
	
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K connection established event

BOOL CDownloadTransferED2K::OnConnected()
{
	ASSERT( m_pClient != NULL );
	ASSERT( m_pSource != NULL );
	
	m_pHost		= m_pClient->m_pHost;
	m_sAddress	= m_pClient->m_sAddress;
	
	m_pSource->m_bGUID		= TRUE;
	m_pSource->m_oGUID		= m_pClient->m_pGUID;
	m_pSource->m_sServer	= m_sUserAgent = m_pClient->m_sUserAgent;
	m_pSource->m_sNick		= m_pClient->m_sNick;
	m_pSource->SetLastSeen();
	
	theApp.Message( MSG_DEFAULT, IDS_DOWNLOAD_CONNECTED, (LPCTSTR)m_sAddress );
	
	return SendPrimaryRequest();
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K connection dropped event

void CDownloadTransferED2K::OnDropped(BOOL bError)
{
	if ( m_nState == dtsQueued )
	{
		theApp.Message( MSG_DEFAULT, IDS_DOWNLOAD_QUEUE_DROP,
			(LPCTSTR)m_pDownload->GetDisplayName() );
	}
	else
	{
		theApp.Message( MSG_ERROR, IDS_DOWNLOAD_DROPPED, (LPCTSTR)m_sAddress );
		Close( TS_UNKNOWN );
	}
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K packet handlers

BOOL CDownloadTransferED2K::OnFileReqAnswer(CEDPacket* pPacket)
{
	// Not really interested
	return TRUE;
}

BOOL CDownloadTransferED2K::OnFileNotFound(CEDPacket* pPacket)
{
	theApp.Message( MSG_ERROR, IDS_DOWNLOAD_FILENOTFOUND,
		(LPCTSTR)m_sAddress, (LPCTSTR)m_pDownload->GetDisplayName() );
	
	Close( TS_FALSE );
	return FALSE;
}

BOOL CDownloadTransferED2K::OnFileStatus(CEDPacket* pPacket)
{
	if ( m_nState <= dtsConnecting ) return TRUE;
	
	if ( pPacket->GetRemaining() < 2 + ED2K_HASH_SIZE )
	{
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	
	CHashED2K oED2K;
	pPacket->Read( oED2K );
	
	if ( ! ( m_pDownload->m_oED2K == oED2K ) )
	{
		theApp.Message( MSG_ERROR, IDS_DOWNLOAD_WRONG_HASH,
			(LPCTSTR)m_sAddress, (LPCTSTR)m_pDownload->GetDisplayName() );
		return TRUE;
	}
	
	DWORD nBlocks = pPacket->ReadShortLE();
	
	if ( nBlocks == (DWORD)( ( m_pDownload->m_nSize + ED2K_PART_SIZE - 1 ) / ED2K_PART_SIZE ) )
	{
		m_pSource->m_oAvailable.Delete();
		
		for ( DWORD nBlock = 0 ; nBlock < nBlocks && pPacket->GetRemaining() ; )
		{
			BYTE nByte = pPacket->ReadByte();
			
			for ( int nBit = 0 ; nBit < 8 && nBlock < nBlocks ; nBit++, nBlock++ )
			{
				if ( nByte & ( 1 << nBit ) )
				{
					QWORD nFrom = ED2K_PART_SIZE * nBlock;
					QWORD nTo = nFrom + ED2K_PART_SIZE;
					nTo = min( nTo, m_pDownload->m_nSize );
					
					m_pSource->m_oAvailable.Add( nFrom, nTo );
				}
			}
		}
	}
	else if ( nBlocks == 0 )
	{
		m_pSource->m_oAvailable.Delete();
	}
	else
	{
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	
	SendSecondaryRequest();
	
	return TRUE;
}

BOOL CDownloadTransferED2K::OnHashsetAnswer(CEDPacket* pPacket)
{
	if ( m_nState != dtsHashset ) return TRUE;
	
	if ( pPacket->GetRemaining() < 2 + ED2K_HASH_SIZE )
	{
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	
	CHashED2K oED2K;
	pPacket->Read( oED2K );
	
	if ( ! ( m_pDownload->m_oED2K == oED2K ) )
	{
		return TRUE;	// Hack
		theApp.Message( MSG_ERROR, IDS_DOWNLOAD_HASHSET_ERROR, (LPCTSTR)m_sAddress );
		Close( TS_FALSE );
		return FALSE;
	}
	
	m_bHashset = TRUE;
	
	DWORD nBlocks = pPacket->ReadShortLE();
	
	if ( nBlocks == 0 ) nBlocks = 1;
	
	if ( nBlocks != (DWORD)( ( m_pDownload->m_nSize + ED2K_PART_SIZE - 1 ) / ED2K_PART_SIZE ) )
	{
		theApp.Message( MSG_ERROR, IDS_DOWNLOAD_HASHSET_ERROR, (LPCTSTR)m_sAddress );
	}
	else if ( m_pDownload->SetHashset(	pPacket->m_pBuffer + pPacket->m_nPosition,
										pPacket->GetRemaining() ) )
	{
		return SendSecondaryRequest();
	}
	
	Close( TS_FALSE );
	return FALSE;
}

BOOL CDownloadTransferED2K::OnQueueRank(CEDPacket* pPacket)
{
	if ( m_nState <= dtsConnecting ) return TRUE;
	
	if ( pPacket->GetRemaining() < 4 )
	{
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	
	m_nQueuePos	= pPacket->ReadLongLE();
	
	if ( m_nQueuePos > 0 )
	{
		SetQueueRank( m_nQueuePos );
	}
	else
	{
		m_pSource->m_tAttempt = GetTickCount() + Settings.eDonkey.ReAskTime * 1000;
		Close( TS_UNKNOWN );
	}
	
	return TRUE;
}

BOOL CDownloadTransferED2K::OnRankingInfo(CEDPacket* pPacket)
{
	if ( m_nState <= dtsConnecting ) return TRUE;
	
	if ( pPacket->GetRemaining() < 12 )
	{
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	
	m_nQueuePos	= pPacket->ReadShortLE();
	m_nQueueLen	= pPacket->ReadShortLE();
	
	SetQueueRank( m_nQueuePos );
	
	return TRUE;
}

BOOL CDownloadTransferED2K::OnStartUpload(CEDPacket* pPacket)
{
	SetState( dtsDownloading );
	m_pClient->m_mInput.tLast = GetTickCount();
	
	ClearRequests();
	
	return SendFragmentRequests();
}

BOOL CDownloadTransferED2K::OnFinishUpload(CEDPacket* pPacket)
{
	return SendPrimaryRequest();
}

BOOL CDownloadTransferED2K::OnSendingPart(CEDPacket* pPacket)
{
	if ( m_nState != dtsDownloading ) return TRUE;
	
	if ( pPacket->GetRemaining() <= 8 + ED2K_HASH_SIZE )
	{
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	
	CHashED2K oED2K;
	pPacket->Read( oED2K );

	if ( ! ( m_pDownload->m_oED2K == oED2K ) )
	{
		theApp.Message( MSG_ERROR, IDS_DOWNLOAD_WRONG_HASH,
			(LPCTSTR)m_sAddress, (LPCTSTR)m_pDownload->GetDisplayName() );
		// Close( TS_FALSE );
		// return FALSE;
		return TRUE;
	}
	
	QWORD nOffset = pPacket->ReadLongLE();
	QWORD nLength = pPacket->ReadLongLE();
	
	if ( nLength <= nOffset )
	{
		if ( nLength == nOffset ) return TRUE;
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	
	nLength -= nOffset;
	
	if ( nLength > (QWORD)pPacket->GetRemaining() )
	{
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	
	BOOL bUseful = m_pDownload->SubmitData( nOffset,
		pPacket->m_pBuffer + pPacket->m_nPosition, nLength );
	
	m_oRequested.Subtract( nOffset, nOffset + nLength );
	
	m_pSource->AddFragment( nOffset, nLength,
		( nOffset % ED2K_PART_SIZE ) ? TRUE : FALSE );
	
	m_nDownloaded += nLength;
	
	m_pSource->SetValid();
	
	return SendFragmentRequests();
}

BOOL CDownloadTransferED2K::OnCompressedPart(CEDPacket* pPacket)
{
	if ( m_nState != dtsDownloading ) return TRUE;
	if ( pPacket->GetRemaining() <= 8 + ED2K_HASH_SIZE )
	{
		theApp.Message( MSG_ERROR, IDS_ED2K_CLIENT_BAD_PACKET, (LPCTSTR)m_sAddress, pPacket->m_nType );
		Close( TS_FALSE );
		return FALSE;
	}
	CHashED2K oED2K;
	pPacket->Read( oED2K );
	if ( ! ( m_pDownload->m_oED2K == oED2K ) )
	{
		theApp.Message( MSG_ERROR, IDS_DOWNLOAD_WRONG_HASH,
			(LPCTSTR)m_sAddress, (LPCTSTR)m_pDownload->GetDisplayName() );
		// Close( TS_FALSE );
		// return FALSE;
		return TRUE;
	}
	QWORD nBaseOffset = pPacket->ReadLongLE();
	QWORD nBaseLength = pPacket->ReadLongLE();
	z_streamp pStream = (z_streamp)m_pInflatePtr;
	if ( m_pInflatePtr == NULL || m_nInflateOffset != nBaseOffset || m_nInflateLength != nBaseLength )
	{
		if ( pStream != NULL )
		{
			inflateEnd( pStream );
			delete pStream;
		}
		m_nInflateOffset	= nBaseOffset;
		m_nInflateLength	= nBaseLength;
		m_nInflateRead		= 0;
		m_nInflateWritten	= 0;
		m_pInflateBuffer->Clear();
		m_pInflatePtr = new z_stream;
		pStream = (z_streamp)m_pInflatePtr;
		ZeroMemory( pStream, sizeof(z_stream) );
		if ( inflateInit( pStream ) != Z_OK )
		{
			delete pStream;
			m_pInflatePtr = NULL;
			
			theApp.Message( MSG_ERROR, IDS_DOWNLOAD_INFLATE_ERROR,
				(LPCTSTR)m_pDownload->GetDisplayName() );
			
			Close( TS_FALSE );
			return FALSE;
		}
	}
	m_pInflateBuffer->Add( pPacket->m_pBuffer + pPacket->m_nPosition, pPacket->GetRemaining() );
	BYTE pBuffer[ BUFFER_SIZE ];
	if ( m_pInflateBuffer->m_nLength > 0 && m_nInflateRead < m_nInflateLength )
	{
		pStream->next_in	= m_pInflateBuffer->m_pBuffer;
		pStream->avail_in	= m_pInflateBuffer->m_nLength;
		do
		{
			pStream->next_out	= pBuffer;
			pStream->avail_out	= BUFFER_SIZE;
			inflate( pStream, Z_SYNC_FLUSH );
			if ( pStream->avail_out < BUFFER_SIZE )
			{
				QWORD nOffset = m_nInflateOffset + m_nInflateWritten;
				QWORD nLength = BUFFER_SIZE - pStream->avail_out;
				BOOL bUseful = m_pDownload->SubmitData( nOffset, pBuffer, nLength );
				m_oRequested.Subtract( nOffset, nOffset + nLength );
				m_pSource->AddFragment( nOffset, nLength, ( nOffset % ED2K_PART_SIZE ) ? TRUE : FALSE );
				m_nDownloaded += nLength;
				m_nInflateWritten += nLength;
			}
		}
		while ( pStream->avail_out == 0 );
		if ( pStream->avail_in >= 0 && pStream->avail_in < m_pInflateBuffer->m_nLength )
		{
			m_nInflateRead += ( m_pInflateBuffer->m_nLength - pStream->avail_in );
			m_pInflateBuffer->Remove( m_pInflateBuffer->m_nLength - pStream->avail_in );
		}
	}
	if ( m_nInflateRead >= m_nInflateLength )
	{
		inflateEnd( pStream );
		delete pStream;
		m_pInflatePtr = NULL;
		m_pInflateBuffer->Clear();
	}
	m_pSource->SetValid();
	return SendFragmentRequests();
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K send

void CDownloadTransferED2K::Send(CEDPacket* pPacket, BOOL bRelease)
{
	ASSERT( m_nState > dtsConnecting );
	ASSERT( m_pClient != NULL );
	m_pClient->Send( pPacket, bRelease );
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K file requestors

BOOL CDownloadTransferED2K::SendPrimaryRequest()
{
	ASSERT( m_pClient != NULL );
	
	/*
	if ( m_pDownload->GetVolumeRemaining() == 0 )
	{
		theApp.Message( MSG_DEFAULT, IDS_DOWNLOAD_FRAGMENT_END, (LPCTSTR)m_sAddress );
		Close( TS_TRUE );
		return FALSE;
	}
	*/
	
	SetState( dtsRequesting );
	m_tRequest	= GetTickCount();
	ClearRequests();
	
	CEDPacket* pPacket = CEDPacket::New( ED2K_C2C_FILEREQUEST );
	pPacket->Write( m_pDownload->m_oED2K );
	if ( Settings.eDonkey.ExtendedRequest && m_pClient->m_bEmRequest >= 1 ) m_pClient->WritePartStatus( pPacket, m_pDownload );
	Send( pPacket );
	
	pPacket = CEDPacket::New( ED2K_C2C_FILESTATUSREQUEST );
	pPacket->Write( m_pDownload->m_oED2K );
	Send( pPacket );
	
	if ( m_pDownload->GetSourceCount() < 500 && m_pClient->m_bEmule && Network.IsListening() )
	{
		pPacket = CEDPacket::New( ED2K_C2C_REQUESTSOURCES, ED2K_PROTOCOL_EMULE );
		pPacket->Write( m_pDownload->m_oED2K );
		Send( pPacket );
	}
	
	return TRUE;
}

BOOL CDownloadTransferED2K::SendSecondaryRequest()
{
	ASSERT( m_pClient != NULL );
	ASSERT( m_nState > dtsConnecting );
	// ASSERT( m_nState == dtsRequesting || m_nState == dtsHashset );
	
	if ( ! m_pDownload->PrepareFile() )
	{
		Close( TS_TRUE );
		return FALSE;
	}
	
	if ( m_bHashset == FALSE && m_pDownload->NeedHashset() )
	{
		CEDPacket* pPacket = CEDPacket::New( ED2K_C2C_HASHSETREQUEST );
		pPacket->Write( m_pDownload->m_oED2K );
		Send( pPacket );
		
		SetState( dtsHashset );
		m_pClient->m_mInput.tLast = GetTickCount();
	}
	else if ( m_pSource->HasUsefulRanges() )
	{
		CEDPacket* pPacket = CEDPacket::New( ED2K_C2C_QUEUEREQUEST );
		pPacket->Write( m_pDownload->m_oED2K );
		Send( pPacket );
		
		SetState( dtsEnqueue );
		m_tRequest = GetTickCount();
	}
	else
	{
		m_pSource->m_tAttempt = GetTickCount() + Settings.eDonkey.ReAskTime * 500;
		m_pSource->SetAvailableRanges( NULL );
		theApp.Message( MSG_DEFAULT, IDS_DOWNLOAD_FRAGMENT_END, (LPCTSTR)m_sAddress );
		Close( TS_TRUE );
		return FALSE;
	}
	
	ClearRequests();
	
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K fragment request manager

BOOL CDownloadTransferED2K::SendFragmentRequests()
{
	ASSERT( m_nState == dtsDownloading );
	ASSERT( m_pClient != NULL );
	if ( m_oRequested.GetCount() >= Settings.eDonkey.RequestPipe ) return TRUE;
	if ( m_pSource->m_oAvailable.IsEmpty() )
	{
		m_oPossible.GetCopy( m_pDownload->m_pFile->m_oFree );
	}
	else
	{
		m_oPossible.GetAnd( m_pDownload->m_pFile->m_oFree, m_pSource->m_oAvailable );
	}
	CDownloadTransfer* pTransfer = m_pDownload->GetFirstTransfer();
	while ( pTransfer && !m_oPossible.IsEmpty() )
	{
		pTransfer->SubtractRequested( m_oPossible );
		pTransfer = pTransfer->m_pDlNext;
	}
	while ( !m_oPossible.IsEmpty() && m_oRequested.GetCount() < Settings.eDonkey.RequestPipe )
	{
		QWORD nOffset, nLength;
		if ( SelectFragment( m_oPossible, nOffset, nLength ) )
		{
			ChunkifyRequest( &nOffset, &nLength, Settings.eDonkey.RequestSize, FALSE );
			m_oPossible.Subtract( nOffset, nOffset + nLength );
			m_oRequested.Add( nOffset, nOffset + nLength );
			CEDPacket* pPacket = CEDPacket::New( ED2K_C2C_REQUESTPARTS );
			pPacket->Write( m_pDownload->m_oED2K );
			pPacket->WriteLongLE( (DWORD)nOffset );
			pPacket->WriteLongLE( 0 );
			pPacket->WriteLongLE( 0 );
			pPacket->WriteLongLE( (DWORD)( nOffset + nLength ) );
			pPacket->WriteLongLE( 0 );
			pPacket->WriteLongLE( 0 );
			Send( pPacket );
			int nType = ( m_nDownloaded == 0 || ( nOffset % ED2K_PART_SIZE ) == 0 )	? MSG_DEFAULT : MSG_DEBUG;
			theApp.Message( nType, IDS_DOWNLOAD_FRAGMENT_REQUEST,
				nOffset, nOffset + nLength - 1,
				(LPCTSTR)m_pDownload->GetDisplayName(), (LPCTSTR)m_sAddress );
		}
		else break;
	}
	if ( m_oRequested.GetCount() > 0 ) return TRUE;
	Send( CEDPacket::New( ED2K_C2C_QUEUERELEASE ) );
	theApp.Message( MSG_DEFAULT, IDS_DOWNLOAD_FRAGMENT_END, (LPCTSTR)m_sAddress );
	Close( TS_TRUE );
	return FALSE;
}

void CDownloadTransferED2K::ClearRequests()
{
	m_oRequested.Delete();
	
	if ( z_streamp pStream = (z_streamp)m_pInflatePtr )
	{
		inflateEnd( pStream );
		delete pStream;
		m_pInflatePtr = NULL;
		m_pInflateBuffer->Clear();
	}
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K fragment selector

BOOL CDownloadTransferED2K::SelectFragment(CFileFragmentList& oPossible, QWORD& nOffset, QWORD& nLength)
{
	if ( oPossible.IsEmpty() ) return FALSE;
	BOOL bAvailable = TRUE;
	DWORD nBlock, nLengthCount, nFound = 0;
	DWORD* aBlocks;
	CFileFragment* pFragment = oPossible.GetFirst();
	do
	{
		if ( pFragment->Offset() % ED2K_PART_SIZE )
		{	// the start of a block is complete, but part is missing
			nOffset = pFragment->Offset();
			nLength = min( ED2K_PART_SIZE - ( pFragment->Offset() % ED2K_PART_SIZE ), pFragment->Length() );
			return TRUE;
		}
		else if ( ( pFragment->Next() % ED2K_PART_SIZE ) && ( pFragment->Next() < m_pDownload->m_nSize ) )
		{	// the end of a block is complete, but part is missing
			nOffset = pFragment->Next() - ( pFragment->Next() % ED2K_PART_SIZE );
			nLength = pFragment->Next() - nOffset;
			return TRUE;
		}
	}
	while ( pFragment = pFragment->GetNext() );
	aBlocks = new DWORD[ (DWORD)(( m_pDownload->m_nSize + ED2K_PART_SIZE - 1 ) / ED2K_PART_SIZE) ];
	pFragment = oPossible.GetFirst();
	do
	{	// all Fragments contain aligned Blocks
		nBlock = (DWORD)( pFragment->Offset() / ED2K_PART_SIZE );
		nLengthCount = (DWORD)( ( pFragment->Length() + ED2K_PART_SIZE - 1 ) / ED2K_PART_SIZE );
		while ( nLengthCount-- ) aBlocks[ nFound++ ] = nBlock++;
	}
	while ( pFragment = pFragment->GetNext() );
	nOffset = (QWORD)(aBlocks[ ( nFound * rand() ) >> 15 ]) * ED2K_PART_SIZE;
	nLength = nOffset + ED2K_PART_SIZE > m_pDownload->m_nSize ? m_pDownload->m_nSize - nOffset : ED2K_PART_SIZE;
	delete [] aBlocks;
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K subtract requested fragments

BOOL CDownloadTransferED2K::SubtractRequested(CFileFragmentList& Fragments)
{
	if ( m_nState == dtsDownloading )
	{
		if ( m_oRequested.GetCount() != 0 ) Fragments.Subtract( m_oRequested );
		return TRUE;
	}
	
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K run queued state

BOOL CDownloadTransferED2K::RunQueued(DWORD tNow)
{
	ASSERT( m_pClient != NULL );
	ASSERT( m_nState == dtsQueued );
	
	if ( Settings.Downloads.QueueLimit > 0 && m_nQueuePos > Settings.Downloads.QueueLimit )
	{
		theApp.Message( MSG_ERROR, IDS_DOWNLOAD_QUEUE_HUGE,
			(LPCTSTR)m_sAddress, (LPCTSTR)m_pDownload->GetDisplayName(), m_nQueuePos );
		Close( TS_FALSE );
		return FALSE;
	}
	else if ( m_pClient->m_bConnected == FALSE && tNow > m_tRanking && tNow - m_tRanking > Settings.eDonkey.ReAskTime * 1000 + 20000 )
	{
		theApp.Message( MSG_ERROR, IDS_DOWNLOAD_QUEUE_TIMEOUT,
			(LPCTSTR)m_sAddress, (LPCTSTR)m_pDownload->GetDisplayName() );
		Close( TS_UNKNOWN );
		return FALSE;
	}
	else if ( m_pClient->m_nUDP > 0 && ! m_bUDP && tNow > m_tRequest && tNow - m_tRequest > Settings.eDonkey.ReAskTime * 1000 - 20000 )
	{
		CEDPacket* pPing = CEDPacket::New( ED2K_C2C_UDP_REASKFILEPING, ED2K_PROTOCOL_EMULE );
		pPing->Write( m_pDownload->m_oED2K );
		Datagrams.Send( &m_pClient->m_pHost.sin_addr, m_pClient->m_nUDP, pPing );
		m_bUDP = TRUE;
	}
	else if ( tNow > m_tRequest && tNow - m_tRequest > Settings.eDonkey.ReAskTime * 1000 )
	{
		m_tRequest = GetTickCount();
		
		if ( m_pClient->IsOnline() )
		{
			return OnConnected();
		}
		else
		{
			m_pClient->Connect();
		}
	}
	
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// CDownloadTransferED2K queue rank update

void CDownloadTransferED2K::SetQueueRank(int nRank)
{
	SetState( dtsQueued );

	m_tRequest	= m_tRanking = GetTickCount();
	m_nQueuePos	= nRank;
	m_bUDP		= FALSE;
	
	ClearRequests();
	
	theApp.Message( MSG_DEFAULT, IDS_DOWNLOAD_QUEUED,
		(LPCTSTR)m_sAddress, m_nQueuePos, m_nQueueLen, _T("eDonkey2000") );
}
