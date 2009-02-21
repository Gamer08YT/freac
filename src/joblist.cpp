 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2009 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#include <joblist.h>
#include <playlist.h>
#include <startgui.h>
#include <dllinterfaces.h>
#include <utilities.h>

#include <gui/layer_tooltip.h>

#include <jobs/job_adddirectory.h>
#include <jobs/job_addfiles.h>
#include <jobs/job_removeall.h>

#include <cddb/cddbremote.h>

using namespace BoCA::AS;
using namespace smooth::IO;

BonkEnc::JobList::JobList(const Point &iPos, const Size &iSize) : ListBox(iPos, iSize)
{
	SetFlags(LF_ALLOWREORDER | LF_MULTICHECKBOX);

	AddTab(BonkEnc::i18n->TranslateString("Title"));
	AddTab(BonkEnc::i18n->TranslateString("Track"), Config::Get()->tab_width_track, OR_RIGHT);
	AddTab(BonkEnc::i18n->TranslateString("Length"), Config::Get()->tab_width_length, OR_RIGHT);
	AddTab(BonkEnc::i18n->TranslateString("Size"), Config::Get()->tab_width_size, OR_RIGHT);

	onRegister.Connect(&JobList::OnRegister, this);
	onUnregister.Connect(&JobList::OnUnregister, this);

	onSelectEntry.Connect(&JobList::OnSelectEntry, this);
	onMarkEntry.Connect(&JobList::OnMarkEntry, this);

	BoCA::JobList::Get()->onComponentAddTrack.Connect(&JobList::AddTrack, this);
	BoCA::JobList::Get()->onComponentSelectTrack.Connect(&JobList::OnComponentSelectTrack, this);
	BoCA::JobList::Get()->onComponentModifyTrack.Connect(&JobList::UpdateTrackInfo, this);

	BoCA::JobList::Get()->doRemoveAllTracks.Connect(&JobList::RemoveAllTracks, this);

	droparea = new DropArea(iPos, iSize);
	droparea->onDropFile.Connect(&JobList::AddTrackByDragAndDrop, this);

	text			= new Text(NIL, iPos - Point(9, 19));

	button_sel_all		= new Button(NIL, ImageLoader::Load("BonkEnc.pci:18"), iPos - Point(19, 4), Size(21, 21));
	button_sel_all->onAction.Connect(&JobList::SelectAll, this);
	button_sel_all->SetFlags(BF_NOFRAME);
	button_sel_all->SetTooltipText(BonkEnc::i18n->TranslateString("Select all"));

	button_sel_none		= new Button(NIL, ImageLoader::Load("BonkEnc.pci:19"), iPos - Point(19, -10), Size(21, 21));
	button_sel_none->onAction.Connect(&JobList::SelectNone, this);
	button_sel_none->SetFlags(BF_NOFRAME);
	button_sel_none->SetTooltipText(BonkEnc::i18n->TranslateString("Select none"));

	button_sel_toggle	= new Button(NIL, ImageLoader::Load("BonkEnc.pci:20"), iPos - Point(19, -24), Size(21, 21));
	button_sel_toggle->onAction.Connect(&JobList::ToggleSelection, this);
	button_sel_toggle->SetFlags(BF_NOFRAME);
	button_sel_toggle->SetTooltipText(BonkEnc::i18n->TranslateString("Toggle selection"));
}

BonkEnc::JobList::~JobList()
{
	BoCA::JobList::Get()->onComponentAddTrack.Disconnect(&JobList::AddTrack, this);
	BoCA::JobList::Get()->onComponentSelectTrack.Disconnect(&JobList::OnComponentSelectTrack, this);
	BoCA::JobList::Get()->onComponentModifyTrack.Disconnect(&JobList::UpdateTrackInfo, this);

	BoCA::JobList::Get()->doRemoveAllTracks.Disconnect(&JobList::RemoveAllTracks, this);

	onRegister.Disconnect(&JobList::OnRegister, this);
	onUnregister.Disconnect(&JobList::OnUnregister, this);

	DeleteObject(droparea);
	DeleteObject(text);

	DeleteObject(button_sel_all);
	DeleteObject(button_sel_none);
	DeleteObject(button_sel_toggle);
}

Int BonkEnc::JobList::GetNOfTracks() const
{
	return tracks.Length();
}

const BoCA::Track &BonkEnc::JobList::GetNthTrack(Int n) const
{
	static Track	 nil(NIL);

	if (n < 0 || GetNOfTracks() <= n) return nil;
	
	/* Entries may have been moved in the joblist,
	 * so get the entry by index instead of position.
	 */
	return *(tracks.Get(GetNthEntry(n)->GetHandle()));
}

Bool BonkEnc::JobList::CanModifyJobList() const
{
	if (BonkEnc::Get()->encoder->IsEncoding())
	{
		Utilities::ErrorMessage("Cannot modify the joblist while encoding!");

		return False;
	}

	return True;
}

Bool BonkEnc::JobList::AddTrack(const Track &iTrack)
{
	Track		*track = new Track(iTrack);

	track->SetOriginalInfo(track->GetInfo());

	ListEntry	*entry	= AddEntry(GetEntryText(*track));

	if (Config::Get()->showTooltips) entry->SetTooltipLayer(new LayerTooltip(*track));

	entry->SetMark(True);

	tracks.Add(track, entry->GetHandle());

	UpdateTextLine();

	BoCA::JobList::Get()->onApplicationAddTrack.Emit(*track);

	return True;
}

Bool BonkEnc::JobList::RemoveNthTrack(Int n)
{
	ListEntry	*entry = GetNthEntry(n);
	Track		*track = tracks.Get(entry->GetHandle());

	/* Remove track from track list and joblist.
	 */
	tracks.Remove(entry->GetHandle());

	if (entry->GetTooltipLayer() != NIL)
	{
		delete entry->GetTooltipLayer();

		entry->SetTooltipLayer(NIL);
	}

	Remove(entry);

	UpdateTextLine();

	/* Notify components and delete track.
	 */
	BoCA::JobList::Get()->onApplicationRemoveTrack.Emit(*track);

	delete track;

	return True;
}

Bool BonkEnc::JobList::RemoveAllTracks()
{
	if (!CanModifyJobList()) return False;

	for (Int i = 0; i < tracks.Length(); i++)
	{
		ListEntry	*entry = GetNthEntry(i);

		if (entry->GetTooltipLayer() != NIL)
		{
			delete entry->GetTooltipLayer();

			entry->SetTooltipLayer(NIL);
		}
	}

	RemoveAllEntries();

	/* Notify components that all tracks will be removed.
	 */
	BoCA::JobList::Get()->onApplicationRemoveAllTracks.Emit();

	for (Int i = 0; i < tracks.Length(); i++)
	{
		delete tracks.GetNth(i);
	}

	tracks.RemoveAll();

	UpdateTextLine();

	return True;
}

Void BonkEnc::JobList::StartJobRemoveAllTracks()
{
	(new JobRemoveAllTracks())->Schedule();
}

const BoCA::Track &BonkEnc::JobList::GetSelectedTrack() const
{
	return GetNthTrack(GetSelectedEntryNumber());
}

Int BonkEnc::JobList::SetMetrics(const Point &nPos, const Size &nSize)
{
	droparea->SetMetrics(nPos, nSize);

	return ListBox::SetMetrics(nPos, nSize);
}

Void BonkEnc::JobList::AddTrackByDialog()
{
	if (!CanModifyJobList()) return;

	FileSelection	*dialog = new FileSelection();

	dialog->SetParentWindow(container->GetContainerWindow());
	dialog->SetFlags(SFD_ALLOWMULTISELECT);

	Array<String>	 types;
	Array<String>	 extensions;

	Registry	&boca = Registry::Get();

	for (Int j = 0; j < boca.GetNumberOfComponents(); j++)
	{
		if (boca.GetComponentType(j) != BoCA::COMPONENT_TYPE_DECODER) continue;

		const Array<FileFormat *>	&formats = boca.GetComponentFormats(j);

		for (Int k = 0; k < formats.Length(); k++)
		{
			const Array<String>	&format_extensions = formats.GetNth(k)->GetExtensions();
			String			 extension;

			for (Int l = 0; l < format_extensions.Length(); l++)
			{
				extension.Append("*.").Append(format_extensions.GetNth(l));

				if (l < format_extensions.Length() - 1) extension.Append("; ");
			}

			types.Add(String(formats.GetNth(k)->GetName()).Append(" (").Append(extension).Append(")"));
			extensions.Add(extension);
		}
	}

	String	 fileTypes;

	for (Int l = 0; l < extensions.Length(); l++)
	{
		if (fileTypes.Find(extensions.GetNth(l)) < 0) fileTypes.Append(l > 0 ? ";" : NIL).Append(extensions.GetNth(l));
	}

	dialog->AddFilter(BonkEnc::i18n->TranslateString("Audio Files"), fileTypes);

	for (Int m = 0; m < types.Length(); m++) dialog->AddFilter(types.GetNth(m), extensions.GetNth(m));

	dialog->AddFilter(BonkEnc::i18n->TranslateString("All Files"), "*.*");

	if (dialog->ShowDialog() == Success())
	{
		Array<String>	 files;

		for (Int i = 0; i < dialog->GetNumberOfFiles(); i++)
		{
			files.Add(dialog->GetNthFileName(i));
		}

		if (files.Length() > 0) (new JobAddFiles(files))->Schedule();
	}

	delete dialog;
}

Void BonkEnc::JobList::AddTrackByDragAndDrop(const String &file)
{
	if (!CanModifyJobList()) return;

	if (File(file).Exists())
	{
		Array<String>	 files;

		files.Add(file);

		(new JobAddFiles(files))->Schedule();
	}
	else if (Directory(file).Exists())
	{
		(new JobAddDirectory(file))->Schedule();
	}
	else
	{
		Utilities::ErrorMessage(String(BonkEnc::i18n->TranslateString("Unable to open file: %1\n\nError: %2")).Replace("%1", File(file).GetFileName()).Replace("%2", BonkEnc::i18n->TranslateString("File not found")));
	}
}

Void BonkEnc::JobList::AddTracksByPattern(const String &directory, const String &pattern)
{
	Directory		 dir = Directory(directory);
	const Array<File>	&files = dir.GetFilesByPattern(pattern);

	if (files.Length() == 0)
	{
		Utilities::ErrorMessage(String(BonkEnc::i18n->TranslateString("No files found matching pattern:")).Append(" ").Append(pattern));
	}
	else
	{
		Array<String>	 jobFiles;

		foreach (File file, files) jobFiles.Add(file);

		(new JobAddFiles(jobFiles))->Schedule();
	}
}

Void BonkEnc::JobList::UpdateTrackInfo(const Track &track)
{
	for (Int i = 0; i < GetNOfTracks(); i++)
	{
		Track	*existingTrack = tracks.Get(GetNthEntry(i)->GetHandle());

		if (existingTrack->GetTrackID() == track.GetTrackID())
		{
			ListEntry	*entry = GetNthEntry(i);

			entry->SetText(GetEntryText(track));

			if (Config::Get()->showTooltips)
			{
				if (entry->GetTooltipLayer() != NIL) delete entry->GetTooltipLayer();

				entry->SetTooltipLayer(new LayerTooltip(track));
			}

			*existingTrack = track;

			break;
		}
	}

	BoCA::JobList::Get()->onApplicationModifyTrack.Emit(track);
}

Void BonkEnc::JobList::RemoveSelectedTrack()
{
	if (!CanModifyJobList()) return;

	if (GetSelectedEntry() == NIL)
	{
		Utilities::ErrorMessage("You have not selected a file!");

		return;
	}

	const Track	&track = GetSelectedTrack();

	for (Int i = 0; i < GetNOfTracks(); i++)
	{
		if (GetNthTrack(i).GetTrackID() == track.GetTrackID())
		{
			if (Length() > 1)
			{
				if (i < Length() - 1) SelectEntry(GetNthEntry(i + 1));
				else		      SelectEntry(GetNthEntry(i - 1));
			}

			RemoveNthTrack(i);

			break;
		}
	}
}

Void BonkEnc::JobList::SelectAll()
{
	for (Int i = 0; i < Length(); i++)
	{
		if (!GetNthEntry(i)->IsMarked()) GetNthEntry(i)->SetMark(True);
	}
}

Void BonkEnc::JobList::SelectNone()
{
	for (Int i = 0; i < Length(); i++)
	{
		if (GetNthEntry(i)->IsMarked()) GetNthEntry(i)->SetMark(False);
	}
}

Void BonkEnc::JobList::ToggleSelection()
{
	for (Int i = 0; i < Length(); i++)
	{
		if (GetNthEntry(i)->IsMarked())	GetNthEntry(i)->SetMark(False);
		else				GetNthEntry(i)->SetMark(True);
	}
}

Void BonkEnc::JobList::LoadList()
{
	if (!CanModifyJobList()) return;

	FileSelection	*dialog = new FileSelection();

	dialog->SetParentWindow(container->GetContainerWindow());

	dialog->AddFilter(String(BonkEnc::i18n->TranslateString("Playlist Files")).Append(" (*.m3u)"), "*.m3u");
	dialog->AddFilter(BonkEnc::i18n->TranslateString("All Files"), "*.*");

	if (dialog->ShowDialog() == Success())
	{
		Playlist	 playlist;
		Array<String>	 files;

		playlist.Load(dialog->GetFileName());

		for (Int i = 0; i < playlist.GetNOfTracks(); i++) files.Add(playlist.GetNthTrackFileName(i));

		(new JobAddFiles(files))->Schedule();
	}

	delete dialog;
}

Void BonkEnc::JobList::SaveList()
{
	FileSelection	*dialog = new FileSelection();

	dialog->SetParentWindow(container->GetContainerWindow());
	dialog->SetMode(SFM_SAVE);
	dialog->SetFlags(SFD_CONFIRMOVERWRITE);
	dialog->SetDefaultExtension("m3u");

	dialog->AddFilter(String(BonkEnc::i18n->TranslateString("Playlist Files")).Append(" (*.m3u)"), "*.m3u");
	dialog->AddFilter(BonkEnc::i18n->TranslateString("All Files"), "*.*");

	if (dialog->ShowDialog() == Success())
	{
		Playlist playlist;

		for (Int i = 0; i < GetNOfTracks(); i++)
		{
			const Track	&track = GetNthTrack(i);
			const Info	&info = track.GetInfo();

			String		 fileName = track.origFilename;

			if (track.isCDTrack)
			{
				for (Int drive = 2; drive < 26; drive++)
				{
					String	 trackCDA = String(" ").Append(":\\track").Append(track.cdTrack < 10 ? "0" : NIL).Append(String::FromInt(track.cdTrack)).Append(".cda");

					trackCDA[0] = drive + 'A';

					InStream	*in = new InStream(STREAM_FILE, trackCDA, IS_READONLY);

					in->Seek(32);

					Int	 trackLength = in->InputNumber(4);

					delete in;

					if (track.length == (trackLength * 2352) / (track.GetFormat().bits / 8))
					{
						fileName = trackCDA;

						break;
					}
				}
			}

			playlist.AddTrack(fileName, String(info.artist.Length() > 0 ? info.artist : BonkEnc::i18n->TranslateString("unknown artist")).Append(" - ").Append(info.title.Length() > 0 ? info.title : BonkEnc::i18n->TranslateString("unknown title")), track.length == -1 ? -1 : Math::Round((Float) track.length / (track.GetFormat().rate * track.GetFormat().channels)));
		}

		playlist.Save(dialog->GetFileName());
	}

	delete dialog;
}

Void BonkEnc::JobList::OnRegister(Widget *container)
{
	container->Add(droparea);
	container->Add(text);

	container->Add(button_sel_all);
	container->Add(button_sel_none);
	container->Add(button_sel_toggle);

	((BonkEncGUI *) BonkEnc::Get())->onChangeLanguageSettings.Connect(&JobList::OnChangeLanguageSettings, this);
}

Void BonkEnc::JobList::OnUnregister(Widget *container)
{
	container->Remove(droparea);
	container->Remove(text);

	container->Remove(button_sel_all);
	container->Remove(button_sel_none);
	container->Remove(button_sel_toggle);

	((BonkEncGUI *) BonkEnc::Get())->onChangeLanguageSettings.Disconnect(&JobList::OnChangeLanguageSettings, this);
}

Void BonkEnc::JobList::OnSelectEntry()
{
	BoCA::JobList::Get()->onApplicationSelectTrack.Emit(GetSelectedTrack());
}

Void BonkEnc::JobList::OnMarkEntry(ListEntry *entry)
{
	if (tracks.Get(entry->GetHandle()) == NIL) return;

	if (entry->IsMarked())	BoCA::JobList::Get()->onApplicationMarkTrack.Emit(*tracks.Get(entry->GetHandle()));
	else			BoCA::JobList::Get()->onApplicationUnmarkTrack.Emit(*tracks.Get(entry->GetHandle()));
}

Void BonkEnc::JobList::OnComponentSelectTrack(const Track &track)
{
	for (Int i = 0; i < GetNOfTracks(); i++)
	{
		const Track	&existingTrack = GetNthTrack(i);

		if (existingTrack.GetTrackID() == track.GetTrackID())
		{
			if (GetSelectedEntryNumber() != i) SelectNthEntry(i);

			break;
		}
	}
}

Void BonkEnc::JobList::OnChangeLanguageSettings()
{
	UpdateTextLine();

	button_sel_all->SetTooltipText(BonkEnc::i18n->TranslateString("Select all"));
	button_sel_none->SetTooltipText(BonkEnc::i18n->TranslateString("Select none"));
	button_sel_toggle->SetTooltipText(BonkEnc::i18n->TranslateString("Toggle selection"));

	Hide();

	for (Int i = 0; i < GetNOfTracks(); i++)
	{
		const Track	&track = GetNthTrack(i);
		ListEntry	*entry = GetNthEntry(i);

		entry->SetText(GetEntryText(track));

		if (Config::Get()->showTooltips)
		{
			if (entry->GetTooltipLayer() != NIL) delete entry->GetTooltipLayer();

			entry->SetTooltipLayer(new LayerTooltip(track));
		}
	}

	Config::Get()->tab_width_track = GetNthTabWidth(1);
	Config::Get()->tab_width_length = GetNthTabWidth(2);
	Config::Get()->tab_width_size = GetNthTabWidth(3);

	RemoveAllTabs();

	AddTab(BonkEnc::i18n->TranslateString("Title"));
	AddTab(BonkEnc::i18n->TranslateString("Track"), Config::Get()->tab_width_track, OR_RIGHT);
	AddTab(BonkEnc::i18n->TranslateString("Length"), Config::Get()->tab_width_length, OR_RIGHT);
	AddTab(BonkEnc::i18n->TranslateString("Size"), Config::Get()->tab_width_size, OR_RIGHT);

	Show();
}

Void BonkEnc::JobList::UpdateTextLine()
{
	text->SetText(String(BonkEnc::i18n->TranslateString("%1 file(s) in joblist:")).Replace("%1", String::FromInt(GetNOfTracks())));
}

const String &BonkEnc::JobList::GetEntryText(const Track &track)
{
	const Info	&info = track.GetInfo();
	static String	 jlEntry;

	if (info.artist == NIL && info.title == NIL) jlEntry = String(track.origFilename).Append("\t");
	else					     jlEntry = String(info.artist.Length() > 0 ? info.artist : BonkEnc::i18n->TranslateString("unknown artist")).Append(" - ").Append(info.title.Length() > 0 ? info.title : BonkEnc::i18n->TranslateString("unknown title")).Append("\t");

	jlEntry.Append(info.track > 0 ? (info.track < 10 ? String("0").Append(String::FromInt(info.track)) : String::FromInt(info.track)) : String(NIL)).Append("\t").Append(track.lengthString).Append("\t").Append(track.fileSizeString);

	return jlEntry;
}
