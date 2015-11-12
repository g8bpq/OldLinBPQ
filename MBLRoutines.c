/*
Copyright 2001-2015 John Wiseman G8BPQ

This file is part of LinBPQ/BPQ32.

LinBPQ/BPQ32 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

LinBPQ/BPQ32 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with LinBPQ/BPQ32.  If not, see http://www.gnu.org/licenses
*/	

// Mail and Chat Server for BPQ32 Packet Switch
//
// MBL-Style Forwarding Routines

#include "BPQMail.h"

VOID ProcessMBLLine(CIRCUIT * conn, struct UserInfo * user, UCHAR* Buffer, int len)
{
	Buffer[len] = 0;

	// Winpack can send a second SID to switch to upload mode

	if (Buffer[0] == '[' && Buffer[len-2] == ']')		// SID
	{
		if (user->flags & (F_PMS))
		{
			Parse_SID(conn, &Buffer[1], len-4);
			
			if (conn->BBSFlags & FBBForwarding)
			{
				conn->FBBIndex = 0;		// ready for first block;
				conn->FBBChecksum = 0;
				memset(&conn->FBBHeaders[0], 0, 5 * sizeof(struct FBBHeaderLine));
			}
			else
				FBBputs(conn, ">\r");
		}

		return;
	}

	if (Buffer[0] == 6 && Buffer[1] == 5)
	{
		// ?? Sally send there after a failed tranfer

		memmove(Buffer, &Buffer[2], len);
		len-=2;
	}


	if (_memicmp(Buffer, "F< ", 3) == 0)
	{
		// FBB Compressed request from system using UI Messages

		int Number = atoi(&Buffer[3]);
		struct MsgInfo * Msg = FindMessageByNumber(Number);
		char ErrMsg[80];
		int ErrLen;

		if (Msg == 0)
		{
			ErrLen = sprintf(&ErrMsg[2], "Msg $%d does not exist!\r>", Number);
			ErrMsg[0] = 0x18;
			ErrMsg[1] = ErrLen;

			BBSputs(conn, ErrMsg);
			FBBputs(conn, ">\r");
			return;
		}
		
		Msg = FindMessage(user->Call, Number, conn->sysop);

		if (Msg)
		{
			SendCompressed(conn, Msg);
			FBBputs(conn, ">\r");
			Msg->status = 'Y';					// Mark as read
		}
		else
		{
			ErrLen = sprintf(&ErrMsg[2], "Msg $%d not available to you!\r>", Number);
			ErrMsg[0] = 0x18;
			ErrMsg[1] = ErrLen;

			BBSputs(conn, ErrMsg);
			FBBputs(conn, ">\r");
		}
		return;
	}

	if (Buffer[0] == 'S')				//Send
	{
		// SB WANT @ ALLCAN < N6ZFJ $4567_N0ARY

		char * Cmd;
		char * To = NULL;
		char * From = NULL;
		char * BID = NULL;
		char * ATBBS = NULL;
		char * ptr, * Context;
		char seps[] = " \t\r";	
	
		Cmd = strtok_s(Buffer, seps, &Context);

		if (Cmd[1] == 0) Cmd[1] = 'P';

		if (RefuseBulls && Cmd[1] == 'B')
		{
			nodeprintfEx(conn, "NO - BULLS NOT ACCEPTED\r");
			if (conn->BBSFlags & OUTWARDCONNECT)
				nodeprintfEx(conn, "F>\r");				// if Outward connect must be reverse forward
			else
				nodeprintfEx(conn, ">\r");

			return;
		}

		To = strtok_s(NULL, seps, &Context);

		ptr = strtok_s(NULL, seps, &Context);

		while (ptr)
		{
			if (strcmp(ptr, "@") == 0)
			{
				ATBBS = _strupr(strtok_s(NULL, seps, &Context));
			}
			else if(strcmp(ptr, "<") == 0)
			{
				From = strtok_s(NULL, seps, &Context);
			}
			else if (ptr[0] == '$')
				BID = &ptr[1];
			else
			{
				nodeprintfEx(conn, "*** Error: Invalid Format\r");				
				return;
			}

			ptr = strtok_s(NULL, seps, &Context);
		}

		if (!From)
		{
			nodeprintfEx(conn, "*** Error: Invalid Format\r");
			return;
		}

		CreateMessage(conn, From, To, ATBBS, toupper(Cmd[1]), BID, NULL);	

		return;
	}


	if (Buffer[0] == 'N')				// Not wanted
	{
		if (conn->FwdMsg)
		{
			// Zap the entry

			clear_fwd_bit(conn->FwdMsg->fbbs, user->BBSNumber);
			set_fwd_bit(conn->FwdMsg->forw, user->BBSNumber);
			conn->UserPointer->ForwardingInfo->MsgCount--;

			conn->FwdMsg->Locked = 0;	// Unlock

		}

		return;
	}

	if (Buffer[0] == 'O')				// Need it (OK)
	{
		struct tm * tm;
		time_t now;
		char * MsgBytes;
		char * MsgPtr;			// In case updated for B2
		int MsgLen;

		if (!conn->FwdMsg)
			return;

		nodeprintfEx(conn, "%s\r", conn->FwdMsg->title);

		MsgBytes = ReadMessageFile(conn->FwdMsg->number);

		if (MsgBytes == 0)
		{
			MsgBytes = _strdup("Message file not found\r");
			conn->FwdMsg->length = strlen(MsgBytes);
		}
	
		MsgPtr = MsgBytes;
		MsgLen = conn->FwdMsg->length;

		// If a B2 Message, remove B2 Header

		if (conn->FwdMsg->B2Flags)
		{		
			// Remove all B2 Headers, and all but the first part.
					
			MsgPtr = strstr(MsgBytes, "Body:");
			
			if (MsgPtr)
			{
				MsgLen = atoi(&MsgPtr[5]);
				MsgPtr= strstr(MsgBytes, "\r\n\r\n");		// Blank Line after headers
	
				if (MsgPtr)
					MsgPtr +=4;
				else
					MsgPtr = MsgBytes;
			
			}
			else
				MsgPtr = MsgBytes;
		}

		MsgLen = RemoveLF(MsgPtr, MsgLen);

		now = time(NULL);

		tm = gmtime(&now);	
	
		nodeprintf(conn, "R:%02d%02d%02d/%02d%02dZ %d@%s.%s %s\r",
			tm->tm_year-100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min,
			conn->FwdMsg->number, BBSName, HRoute, RlineVer);

		if (memcmp(MsgPtr, "R:", 2) != 0)    // No R line, so must be our message
			BBSputs(conn, "\r");

		QueueMsg(conn, MsgPtr, MsgLen);

		if (user->ForwardingInfo->SendCTRLZ)
			nodeprintf(conn, "\rx1a");
		else
			nodeprintf(conn, "\r/ex\r");

		free(MsgBytes);
			
		conn->FBBMsgsSent = TRUE;

		return;
	}

	if (_stricmp(Buffer, "F>\r") == 0)
	{
		// Reverse forward request

		// If we have just sent a nessage, Flag it as sent

		if (conn->FBBMsgsSent)
		{
			conn->FBBMsgsSent = FALSE;
			clear_fwd_bit(conn->FwdMsg->fbbs, user->BBSNumber);
			set_fwd_bit(conn->FwdMsg->forw, user->BBSNumber);

			//  Only mark as forwarded if sent to all BBSs that should have it
			
			if (memcmp(conn->FwdMsg->fbbs, zeros, NBMASK) == 0)
			{
				conn->FwdMsg->status = 'F';			// Mark as forwarded
				conn->FwdMsg->datechanged=time(NULL);
			}

			conn->FwdMsg->Locked = 0;	// Unlock

			conn->UserPointer->ForwardingInfo->MsgCount--;
		}

		// Send Message or Disconnect

		if (FindMessagestoForward(conn))
		{
			struct MsgInfo * Msg;
				
			// Send S line and wait for response - SB WANT @ USA < W8AAA $1029_N0XYZ 

			Msg = conn->FwdMsg;
		
			nodeprintfEx(conn, "S%c %s @ %s < %s $%s\r", Msg->type, Msg->to,
					(Msg->via[0]) ? Msg->via : conn->UserPointer->Call, 
					Msg->from, Msg->bid);

			conn->BBSFlags |= MBLFORWARDING;
			return;
		}

		BBSputs(conn, "*** DONE\r");
		Flush(conn);
		Sleep(400);
		Disconnect(conn->BPQStream);
		return;
	}

	if (Buffer[len-2] == '>')
	{
		// If we have just sent a nessage, Flag it as sent

		if (conn->FBBMsgsSent)
		{
			conn->FBBMsgsSent = FALSE;

			clear_fwd_bit(conn->FwdMsg->fbbs, user->BBSNumber);
			set_fwd_bit(conn->FwdMsg->forw, user->BBSNumber);

			//  Only mark as forwarded if sent to all BBSs that should have it
			
			if (memcmp(conn->FwdMsg->fbbs, zeros, NBMASK) == 0)
			{
				conn->FwdMsg->status = 'F';			// Mark as forwarded
				conn->FwdMsg->datechanged=time(NULL);
			}

			conn->UserPointer->ForwardingInfo->MsgCount--;
		}

		// Send Message or request reverse using MBL-style forwarding

		if (FindMessagestoForward(conn))
		{
			struct MsgInfo * Msg;
				
			// Send S line and wait for response - SB WANT @ USA < W8AAA $1029_N0XYZ 

			Msg = conn->FwdMsg;
		
			nodeprintfEx(conn, "S%c %s @ %s < %s $%s\r", Msg->type, Msg->to,
					(Msg->via[0]) ? Msg->via : conn->UserPointer->Call, 
					Msg->from, Msg->bid);
			
			return;

		}
		else
		{
			FBBputs(conn, "F>\r");
			return;
		}
	}

	// Winpack after doing ocmpressed downloads sends KM or B

	if (_stricmp(Buffer, "*** DONE\r") == 0 || _stricmp(Buffer, "*** What?\r") == 0 
		|| _stricmp(Buffer, "B\r") == 0)
	{
		Disconnect(conn->BPQStream);
		return;
	}

	if (_stricmp(Buffer, "KM\r") == 0)
	{
		int i;
		struct MsgInfo * Msg;

		for (i = NumberofMessages; i > 0; i--)
		{
			Msg = MsgHddrPtr[i];

			if ((_stricmp(Msg->to, user->Call) == 0))
			{
				if (Msg->type == 'P' && Msg->status == 'Y')
				{
					FlagAsKilled(Msg);
					nodeprintfEx(conn, "Message #%d Killed\r", Msg->number);
				}
			}
		}

		SendPrompt(conn, user);
		return;
	}
}

