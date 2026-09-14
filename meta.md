# ed2k .part.met file format

**v1.0**
Last updated on 16 of October of 2003

© 2003 Iván Montes aka Dr.Slump
drslump@drslump.biz

## Contents

1. **Overview**
   1. Licence
   2. About this document
   3. About the author
   4. About the ed2k p2p network
2. **Part.Met file format**
   1. Introduction
   2. Description of the 14.0 version
   3. Description of the 14.1 version
3. **Meta Tags**
   1. Introduction
   2. Special Meta Tags
   3. The Gap Meta Tags
   4. Known Meta Tags
4. **Appendix**
   1. Frequently Answered Questions. FAQ
   2. Glossary
   3. Document History
   4. Addendum: modern format extensions (aMule, 2026)

---

## 1 Overview

### 1.1 Licence

You may copy, modify, merge, use, publish and/or distribute this document and the information in it contained, as soon as you comply to the following conditions:

- My name and contact address, email, shall be included in all copies or substantial portions of this document.
- A link to the place where you get this file from or it's home site should be included too.

THE DOCUMENT IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE DOCUMENT OR THE USE OR OTHER DEALINGS IN THE DOCUMENT.

### 1.2 About this document

Trying to find a reliable source of information about the file format of the eDonkey p2p program's .met files, I couldn't find any document that explained it in detail. All I found was little incomplete references in dispersed places around the *www*. I had the file format written in an ugly text file, which wasn't properly formatted and of difficult reading.

This document was written to be an easy to read and almost complete reference about the *.part.met* file format. I wrote it mainly for myself, although maybe others can benefice from it.

To discover the file format I took as base the great *ed2k_python* scripts by *Bryn Davies*. Thanks to that information and the help from a nice hex editor and an expression evaluator, I could figure out most of the file format. Actually it's a very simple file format so it haven't been a very difficult task.

You can always find the most up-to-date copy of this document at my home site (*www.drslump.biz*), check among the programming links.

**References**

- ed2k_python, by Bryn Davies — http://ed2k-tools.sourceforge.net
- eMule's source code — http://www.emule-project.net
- Didier Dobrovitch's edWatch's source code — http://edwatch.cjb.net

### 1.3 About the author

My name is Iván Montes Velencoso. I was born in Barcelona, Spain, on the 25th of June of 1980.

Currently I'm studying *Computer Systems' Administration* at a local college. I've been with computers since I was the age of ten, I like to program, post on some bulletin boards, play some WWII first person shooters and chat with my friends abroad. On the Internet I use the nickname *Dr.Slump*, it's not very original so there are hundreds of people with that nick around `:-(`

Another of my greatest interests is football, I play in an indoor football team at my home town. I'm not very skillful with the ball, but I know how to put myself on to the right place of the court and distribute the ball quickly among my team mates.

My personal web page is *www.drslump.biz*, I don't maintain it as much as I would like but I don't feel like wasting my free time talking about myself, even if I like to do so!

### 1.4 About the ed2k p2p network

extract from the eDonkey2000 documentation:

> *eDonkey2000 is a distributed file sharing network. This means that it will give you the opportunity to share files you have on your computer with other users on the network. And you will be able to download files from them. Clients from around the world connect to each other and form the eDonkey2000 network. Through these connections users can search for and download files from any other client on the network.*

---

## 2 Part.Met file format

### 2.1 Introduction

As of the time of writing this, there are two different versions of *.part.met* files that I'm aware of: **14.0** and **14.1**. The first one was used in the official eDonkey and Overnet clients prior to the 0.49 release and it's still being used by the eMule and it's several ports and mods.

The second version, *14.1*, is used on the official eDonkey and Overnet clients since the 0.49 release.

You might be thinking: why the version number is so high (`14.x`)? In fact I'm only speculating here, but prior to the release of Overnet the eDonkey followed a rather strange version naming. The first eDonkey to create `14.0` *.part.met* files was the `24.14.39` one, where `39` is the number of public builds and the others I have no idea. So I guess that the 14 comes from there.

Both versions are quite similar to each other, but they are not binary compatible. What I mean is that you can't use the 14.0 version with a client featuring the 14.1 one, and vice versa. Even if the version number tells us that's it's a minor revision, the author of the file format made some changes on the layout of the file.

I'll try to specify both formats in as much detail as possible, however the file format has been figured out by a trial-error procedure. So there is always the chance that the information or descriptions contained in this document are mistaken.

> **Warning!**
> The v14.1 file format hasn't been tested properly, and there are some fields (data) which I have no clue about what they mean. However the current specification should be enough to handle those files, at least, in a basic way.

### 2.2 Description of the 14.0 version

File format used in the official eDonkey and Overnet clients prior to version 0.49 and by eMule and its ports/mods. MLDonkey probably uses this format too, although I'm not certain about that.

The data file that stores the file being downloaded has an extension of *.part*, the name is a number in the range [0, 2³¹-1], leading zeros are allowed at least on later clients. Actually the name can be any valid filename, however this can arise problems with some clients so it's wise to use only numbers. The name of the *.part* and *.part.met* should be the same to avoid problems.

All the numbers are decimal (base 10). **Pos** is the position in the file and **Size** is in bytes.

| Pos | Type  | Size | Name    | Description |
|-----|-------|------|---------|-------------|
| 0   | int   | 1    | Version | Version number of the .part.met file. Must be 224 (14.0) |
| 1   | int   | 4    | Date    | Last modification of the .part.met file. Unknown Format! |
| 5   | array | 16   | ID hash | This is the MD4 hash of the set of MD4 hashes from the file blocks (chunks) in which is divided the file. It is the same one found in the ed2k links |
| 21  | int   | 2    | Blocks  | The number of block hashes stored in the .part.met |
| 23  | array | 16*N | Hashes  | Array of N elements, where N is the value of Blocks, containing the file blocks' MD4 hashes |
| *   | int   | 4    | NumTags | Number of Meta Tags stored in the .part.met file |
| *   | array | M*N  | MetaTags| Array of N elements, where N is the value of NumTags, containing the Meta Tags (see section 3) |

**Example algorithm to parse the file**

```
f = OpenFile( '001.part.met' );
version = ReadByte( f );

if (version<>224) {
    print( 'Does not seem to be a valid .part.met file' );
    Halt;
}

date = ReadDWord( f );
ed2kHash = ReadByte( f, 16 );  //read 16 bytes
blocks = ReadWord( f );

for (i=0; i<blocks; i++) {
    blockHash[ i ] = ReadByte( f, 16 );
}

NumTags = ReadDWord( f );
//read the tags to the end of the file. See section 3 to parse the tags
Tags = ReadByte( f, filesize(f)-filepos(f) );
```

### 2.3 Description of the 14.1 version (eDonkey/Overnet >0.49)

This is the file format used by the official eDonkey and Overnet clients since version 0.49.

The file being downloaded is stored in N files, where N is the number of file's blocks, with a maximum size of 9500Kb each. All those files have the extension *.part* and should be placed on a folder with the name of the file being downloaded, the *.part.met* should be in that folder too. The name of the *.part* and *.part.met* should be the same. Usually its always '1' since every download is placed on a different folder. In the case that two or more different downloads (different ed2k hash) share the same filename and so the same folder, then the name should be a number in the range [1..N³¹-1].

All the numbers are decimal (base 10). **Pos** is the position in the file and **Size** is in bytes.

| Pos | Type  | Size | Name       | Description |
|-----|-------|------|------------|-------------|
| 0   | int   | 1    | Version    | Version number of the .part.met file. Must be 225 (14.1) |
| 1   | int   | 1    | Unknown1   | Unknown attribute. It's value seems to be always 2, perhaps a kind of version number? |
| 2   | int   | 4    | Date       | Last modification of the .part.met file. Unknown format! |
| 6   | array | 16   | ID hash    | This is the MD4 hash of the set of MD4 hashes from the file blocks (chunks) in which is divided the .part file. It is the same one found in the ed2k links |
| 22  | int   | 4    | NumTags    | Number of Meta Tags stored in the .part.met file |
| 26  | array | M*N  | MetaTags   | Array of N elements, where N is the value of NumTags, containing the Meta Tags (see section 3) |
| *   | int   | 1    | HaveHashes | 0 = No hashes stored; 1 = Hashes stored |
| *   | array | 16*N | Hashes     | Array of N elements, where N is the number of blocks in which is divided the .part file, containing the file blocks' MD4 hashes. This field is optional |
| *   | array | M    | Unknown2   | Unknown data. Perhaps hashes of peers IP's known to have the file? This field is optional |

**Example algorithm to parse the file**

```
f = OpenFile( '1.part.met' );
version = ReadByte( f );

if (version<>224) {
    print( 'Does not seem to be a valid .part.met file' );
    Halt;
}

unknown1 = ReadByte( f );
date = ReadDWord( f );
ed2kHash = ReadByte( f, 16 );  //read 16 bytes
NumTags = ReadDWord( f );

for (i=0; i<NumTags; i++) {
    //see section 3 to parse the tags!
}

HaveHashes = ReadByte( f );
if (HaveHashes = 1) {
    //calculate the number of blocks
    blocks = MetaTag[ FILESIZE ] / 9728000;
    if ( (MetaTag[ FILESIZE ] modulus 9728000) > 0 ) blocks++;

    //read the hashes
    for (i=0; i<blocks; i++) {
        blockHash[ i ] = ReadByte( f, 16 );
    }
}
```

---

## 3 Meta Tags

### 3.1 Introduction

The Meta Tags are used to store information about the download. Things like the file name, file size or which parts of the file haven't been downloaded yet are stored in the .part.met file as meta tags.

A meta tag is composed of a name and a value, the creator/editor of the file is free to add its own meta tags, but there are some reserved tag names. Those reserved tag names are called Special Meta Tags (see 3.2), another reserved tags are the Gap Meta Tags (see 3.3).

Apart from the special meta tags there are other standard tags, for example codec, bitrate or Artist. These meta tags have been adopted as a pseudo standard (see 3.4).

| Pos     | Type  | Size | Name        | Description |
|---------|-------|------|-------------|-------------|
| 0       | int   | 1    | Type        | 2 = String; 3 = Integer |
| 1       | int   | 2    | NameLength  | Size of the string containing the name of the tag |
| 3       | array | N    | Name        | String containing the name of the tag |
| 3+N     | int   | 2    | ValueLength | *(if Type = 2 — get a string value)* Size of the string containing the value of the tag |
| 5+N     | array | M    | Value       | String containing the value of the tag |
| 3+N     | int   | 4    | Value       | *(if Type = 3 — get an integer value)* Value of the tag as a 4 bytes integer number |

**Example algorithm to parse the meta tags**

```
for ( i=0; i<NumTags; i++) {
    Tag[i].Type = ReadByte( f );
    Tag[i].NameLength = ReadWord( f );
    Tag[i].Name = ReadByte( f, NameLength );

    //get a string value
    if ( Tag[i].Type = 2 ) {
        Tag[i].ValueLength = ReadWord( f );
        Tag[i].ValueStr = ReadByte( f, Tag[i].ValueLength );
    }
    else {
        //get an integer value
        if ( Tag[i].Type = 3 ) {
            Tag[i].ValueInt = ReadDWord( f );
        }
        else {
            //unrecognized tag type
            Print( 'Error: Tag type not recognized!' );
            Halt;
        }
    }
}
```

### 3.2 Special Meta Tags

The Special meta tags are those with a name length of 1 byte. To know the meaning of those check the table below.

| value | Type   | Description |
|-------|--------|-------------|
| 1     | String | Filename of the download |
| 2     | Int    | Size in bytes of the download |
| 3     | String | Type of file |
| 4     | String | Format of the file |
| 5     | ???    | Last time the file was seen complete on the network (eMule) |
| 8     | Int    | Number of bytes downloaded so far |
| 18    | String | Name of the temporal (.part) file. Empty on 14.1 |
| 19    | Int    | Priority of the download (eDonkey/Overnet <0.49) |
| 20    | Int    | Status of the download |
| 24    | Int    | Priority of the download (eMule, eDonkey/Overnet > 0.49) |
| 25    | Int    | Priority for uploading (eMule) |

**20: Status of the download**

| value | Description |
|-------|-------------|
| 0     | Ready |
| 1     | Empty |
| 2     | Waiting for hash |
| 3     | Hashing |
| 4     | Error |
| 6     | Unknown |
| 7     | Paused |
| 8     | Completing |
| 9     | Completed |

**19, 20, 21: Priority**

| value | Description |
|-------|-------------|
| 0     | Low |
| 1     | Normal |
| 2     | High |
| 3     | Very high (eMule) / Highest/Horde (eDonkey/Overnet) |
| 4     | Very low (eMule) |
| 5     | Auto (eMule) |

### 3.3 The Gap Meta Tags

The Gap meta tags are a kind of special meta tags (see 3.2).

These tags are used to specify the areas of a file that are missing, in other words, that haven't been downloaded yet.

While Special meta tags have a name length of one the Gap tags have a length of two or more. To recognize them we have to look at the first byte (char) of the name, if it's 9 (start) or 10 (end) then it's a gap. The n bytes remaining are the Gap Reference Number which is expressed as a number in text format.

**Gap meta tags**

| value | Description |
|-------|-------------|
| 9     | Start of a gap |
| 10    | End of a gap |

Each start gap defined must have a matching end gap, to match start and end gaps we put a reference number after each start/end gap identifier. So 9`"1023"` will match 10`"1023"` even if there are other gap tags between them with different reference numbers (i.e: 9`"317"`).

However it'd be a good idea to try and keep them in order.

### 3.4 Known Meta Tags

These tags are usually found on .part.met files. When you look for one of them you should make a case-insensitive comparison, since we never can be sure what case was used for the tag.

| Name    | Type   | Description |
|---------|--------|-------------|
| Artist  | String | The artist of the media file |
| Album   | String | The album of the media file |
| Title   | String | Title of the media file |
| length  | ???    | Length of the media file |
| bitrate | ???    | Bit Rate of the media file |
| codec   | Int    | Codec used in the media file |

---

## 4 Appendix

### 4.1 Frequently Asked Question. FAQ

There are no question to answer yet.

### 4.2 Glossary

**Hash**: A hash is for the data as a finger print for an individual. It claims to give a near unique N bits representation of an infinite number of bits.

**MD4** *(Message Digest 4)*: It's a 128bit hash algorithm used for very data integrity and digital signature.

**p2p** *(Peer to Peer)*: Technology that allows the connection of several clients to exchange information. You download the information directly from other clients' PCs.

### 4.3 Document History

- **16th October 2004 (v1.0)**: Corrected the spelling mistakes and some grammar constructs. PDF version created.
- **15th October 2004 (v1.0rc1)**: The document has been formatted using HTML/CSS.
- **14th October 2004 (v0.1)**: Recollected all the related information from several sources.

### 4.4 Addendum: modern format extensions (aMule, 2026)

> This section is **not** part of the original 2003 document. It was added after verifying the current source code of the [aMule](https://amule-org.github.io/) project (`amule-org/amule` on GitHub, a fork of `amule-project/amule` that resumed active maintenance), to check whether the *.part.met* format described above is still accurate.

The classic tag layout (**Type**, **NameLength**, **Name**, **Value**) described in section 3 is still used by part.met files today. However, two things have changed since 2003:

**A third file version, for large files (>4GB)**

`src/include/common/DataFileVersion.h`:

```c
enum PartMetFileVersions {
    PARTFILE_VERSION           = 0xe0, // 224 — "14.0" in this document
    PARTFILE_SPLITTEDVERSION   = 0xe1, // 225 — "14.1" in this document
    PARTFILE_VERSION_LARGEFILE = 0xe2  // 226 — NOT in this document
};
```

Version `0xE2` (226) uses the **same header layout as 14.0** (version byte, date, ID hash, blocks, hashes, NumTags, tags) — it is not a structural revision, just a version-byte marker meaning "the `FILESIZE` tag of this file may be encoded as a 64-bit integer" (needed once file sizes exceed the 32-bit `UInt32` range).

**More Meta Tag types than String/Integer**

The original document only describes `Type = 2` (String) and `Type = 3` (Integer). The current tag type enum (`src/include/tags/TagTypes.h`) defines many more:

| Value | Type | Value layout in the file |
|-------|------|---------------------------|
| 0x01 | Hash16 | 16 raw bytes (MD4 hash) |
| 0x02 | String | 2-byte length + bytes |
| 0x03 | UInt32 | 4 bytes |
| 0x04 | Float32 | 4 bytes |
| 0x05 | Bool | 1 byte |
| 0x06 | BoolArray | 2-byte bit count + `(bits/8)+1` packed bytes |
| 0x07 | Blob | 4-byte length + bytes |
| 0x08 | UInt16 | 2 bytes |
| 0x09 | UInt8 | 1 byte |
| 0x0A | BSOB | 1-byte length + bytes |
| 0x0B | UInt64 | 8 bytes |
| 0x11–0x20 | Str1…Str16 (compressed string) | *N* bytes, where *N* = type − 0x10 (no length prefix) |

Only `Hash16`, `String`, `UInt32`, `UInt64`, `UInt16`, `UInt8`, `Float32`, `Blob` and `BSOB` are actually written into part.met files by aMule today (`CFileDataIO::WriteTag`, `src/SafeFile.cpp`); `Bool`, `BoolArray` and the compressed `Str*` types are part of the shared tag reader (also used for eD2k/Kad network packets) but are not emitted to disk by aMule — they are supported here defensively, in case another client writes them.

**Sources**: [`amule-org/amule`](https://github.com/amule-org/amule), files `src/include/common/DataFileVersion.h`, `src/include/tags/TagTypes.h`, `src/Tag.cpp`, `src/SafeFile.cpp`, `src/PartFile.cpp` (checked 2026-09-14).

---

All the products and brand names contained in this document are copyright of their respective owners.

© 2003 Iván Montes Velencoso
