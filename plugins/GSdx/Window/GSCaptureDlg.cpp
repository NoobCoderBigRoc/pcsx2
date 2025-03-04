/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSdx.h"
#include "GSCaptureDlg.h"

#define BeginEnumSysDev(clsid, pMoniker) \
	{CComPtr<ICreateDevEnum> pDevEnum4$##clsid; \
	pDevEnum4$##clsid.CoCreateInstance(CLSID_SystemDeviceEnum); \
	CComPtr<IEnumMoniker> pClassEnum4$##clsid; \
	if(SUCCEEDED(pDevEnum4$##clsid->CreateClassEnumerator(clsid, &pClassEnum4$##clsid, 0)) \
	&& pClassEnum4$##clsid) \
	{ \
		for(CComPtr<IMoniker> pMoniker; pClassEnum4$##clsid->Next(1, &pMoniker, 0) == S_OK; pMoniker = NULL) \
		{ \

#define EndEnumSysDev }}}

void GSCaptureDlg::InvalidFile()
{
	const std::wstring message = L"GSdx couldn't open file for capturing: " + m_filename + L".\nCapture aborted.";
	MessageBox(GetActiveWindow(), message.c_str(), L"GSdx System Message", MB_OK | MB_SETFOREGROUND);
}

GSCaptureDlg::GSCaptureDlg()
	: GSDialog(IDD_CAPTURE)
{
	m_width = theApp.GetConfigI("CaptureWidth");
	m_height = theApp.GetConfigI("CaptureHeight");
	m_filename = convert_utf8_to_utf16(theApp.GetConfigS("CaptureFileName"));
}

int GSCaptureDlg::GetSelCodec(Codec& c)
{
	INT_PTR data = 0;

	if(ComboBoxGetSelData(IDC_CODECS, data))
	{
		if(data == 0) return 2;

		c = *(Codec*)data;

		if(!c.filter)
		{
			c.moniker->BindToObject(NULL, NULL, __uuidof(IBaseFilter), (void**)&c.filter);

			if(!c.filter) return 0;
		}

		return 1;
	}

	return 0;
}

void GSCaptureDlg::UpdateConfigureButton()
{
	Codec c;
	bool enable = false;

	if (GetSelCodec(c) != 1)
	{
		EnableWindow(GetDlgItem(m_hWnd, IDC_CONFIGURE), false);
		return;
	}

	if (CComQIPtr<ISpecifyPropertyPages> pSPP = c.filter)
	{
		CAUUID caGUID;
		memset(&caGUID, 0, sizeof(caGUID));
		enable = SUCCEEDED(pSPP->GetPages(&caGUID));
	}
	else if (CComQIPtr<IAMVfwCompressDialogs> pAMVfWCD = c.filter)
	{
		enable = pAMVfWCD->ShowDialog(VfwCompressDialog_QueryConfig, nullptr) == S_OK;
	}
	EnableWindow(GetDlgItem(m_hWnd, IDC_CONFIGURE), enable);
}

void GSCaptureDlg::OnInit()
{
	__super::OnInit();

	SetTextAsInt(IDC_WIDTH, m_width);
	SetTextAsInt(IDC_HEIGHT, m_height);
	SetText(IDC_FILENAME, m_filename.c_str());

	m_codecs.clear();

	_bstr_t selected = theApp.GetConfigS("CaptureVideoCodecDisplayName").c_str();

	ComboBoxAppend(IDC_CODECS, "Uncompressed", 0, true);
	ComboBoxAppend(IDC_COLORSPACE, "YUY2", 0, true);
	ComboBoxAppend(IDC_COLORSPACE, "RGB32", 1, false);

	CoInitialize(0); // this is obviously wrong here, each thread should call this on start, and where is CoUninitalize?

	BeginEnumSysDev(CLSID_VideoCompressorCategory, moniker)
	{
		Codec c;

		c.moniker = moniker;

		std::wstring prefix;

		LPOLESTR str = NULL;

		if(FAILED(moniker->GetDisplayName(NULL, NULL, &str)))
			continue;

		if(wcsstr(str, L"@device:dmo:")) prefix = L"(DMO) ";
		else if(wcsstr(str, L"@device:sw:")) prefix = L"(DS) ";
		else if(wcsstr(str, L"@device:cm:")) prefix = L"(VfW) ";

		c.DisplayName = str;

		CoTaskMemFree(str);

		CComPtr<IPropertyBag> pPB;

		if(FAILED(moniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPB)))
			continue;

		_variant_t var;

		if(FAILED(pPB->Read(_bstr_t(_T("FriendlyName")), &var, NULL)))
			continue;

		c.FriendlyName = prefix + var.bstrVal;

		m_codecs.push_back(c);

		ComboBoxAppend(IDC_CODECS, c.FriendlyName.c_str(), (LPARAM)&m_codecs.back(), c.DisplayName == selected);
	}
	EndEnumSysDev
	UpdateConfigureButton();
}

bool GSCaptureDlg::OnCommand(HWND hWnd, UINT id, UINT code)
{
	switch (id)
	{
	case IDC_FILENAME:
	{
		EnableWindow(GetDlgItem(m_hWnd, IDOK), GetText(IDC_FILENAME).length() != 0);
		return false;
	}
	case IDC_BROWSE:
	{
		if (code == BN_CLICKED)
		{
			wchar_t buff[MAX_PATH] = { 0 };

			OPENFILENAME ofn;
			memset(&ofn, 0, sizeof(ofn));

			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = m_hWnd;
			ofn.lpstrFile = buff;
			ofn.nMaxFile = countof(buff);
			ofn.lpstrFilter = L"Avi files (*.avi)\0*.avi\0";
			ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

			wcscpy(ofn.lpstrFile, m_filename.c_str());
			if (GetSaveFileName(&ofn))
			{
				m_filename = ofn.lpstrFile;
				SetText(IDC_FILENAME, m_filename.c_str());
			}

			return true;
		}
		break;
	}
	case IDC_CONFIGURE:
	{
		if (code == BN_CLICKED)
		{
			Codec c;
			if (GetSelCodec(c) == 1)
			{
				if (CComQIPtr<ISpecifyPropertyPages> pSPP = c.filter)
				{
					CAUUID caGUID;
					memset(&caGUID, 0, sizeof(caGUID));

					if (SUCCEEDED(pSPP->GetPages(&caGUID)))
					{
						IUnknown* lpUnk = NULL;
						pSPP.QueryInterface(&lpUnk);
						OleCreatePropertyFrame(m_hWnd, 0, 0, c.FriendlyName.c_str(), 1, (IUnknown**)&lpUnk, caGUID.cElems, caGUID.pElems, 0, 0, NULL);
						lpUnk->Release();

						if (caGUID.pElems)
							CoTaskMemFree(caGUID.pElems);
					}
				}
				else if (CComQIPtr<IAMVfwCompressDialogs> pAMVfWCD = c.filter)
				{
					if (pAMVfWCD->ShowDialog(VfwCompressDialog_QueryConfig, NULL) == S_OK)
						pAMVfWCD->ShowDialog(VfwCompressDialog_Config, m_hWnd);
				}
			}
			return true;
		}
		break;
	}
	case IDC_CODECS:
	{
		UpdateConfigureButton();
		break;
	}
	case IDOK:
	{
		m_width = GetTextAsInt(IDC_WIDTH);
		m_height = GetTextAsInt(IDC_HEIGHT);
		m_filename = GetText(IDC_FILENAME);
		ComboBoxGetSelData(IDC_COLORSPACE, m_colorspace);

		Codec c;
		int ris = GetSelCodec(c);
		if (ris == 0)
			return false;

		m_enc = c.filter;

		theApp.SetConfig("CaptureWidth", m_width);
		theApp.SetConfig("CaptureHeight", m_height);
		theApp.SetConfig("CaptureFileName", convert_utf16_to_utf8(m_filename).c_str());

		if (ris != 2)
			theApp.SetConfig("CaptureVideoCodecDisplayName", c.DisplayName);
		else
			theApp.SetConfig("CaptureVideoCodecDisplayName", "");
		break;
	}
	default:
		break;
	}
	return __super::OnCommand(hWnd, id, code);
}
