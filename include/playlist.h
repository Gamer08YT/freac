 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2009 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#ifndef H_BONKENC_PLAYLIST
#define H_BONKENC_PLAYLIST

#include <smooth.h>

using namespace smooth;

namespace BonkEnc
{
	class Playlist
	{
		private:
			Array<String>	 fileNames;
			Array<String>	 trackNames;
			Array<Int>	 trackLengths;
		public:
			Bool		 AddTrack(const String &, const String &, Int);

			Int		 GetNOfTracks();
			String		 GetNthTrackFileName(Int);

			Bool		 Save(const String &);
			Bool		 Load(const String &);
	};
};

#endif
