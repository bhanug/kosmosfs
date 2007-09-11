/*
 * $Id: //depot/SOURCE/OPENSOURCE/kfs/src/cc/meta/restore.cc#3 $
 *
 * \file restore.cc
 * \brief rebuild metatree from saved checkpoint
 * \author Blake Lewis (Kosmix Corp.)
 *
 * Copyright (C) 2006 Kosmix Corp.
 *
 * This file is part of Kosmos File System (KFS).
 *
 * KFS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <map>
#include <cerrno>
#include <cstring>
#include "restore.h"
#include "util.h"
#include "logger.h"
#include "meta.h"
#include "kfstree.h"
#include "replay.h"
#include "restore.h"
#include "entry.h"
#include "checkpoint.h"
#include "LayoutManager.h"

using namespace KFS;

static bool
checkpoint_seq(deque <string> &c)
{
	c.pop_front();
	if (c.empty())
		return false;
	seq_t highest = toNumber(c.front());
	oplog.set_seqno(highest);
	return (highest >= 0);
}

static bool
checkpoint_fid(deque <string> &c)
{
	c.pop_front();
	if (c.empty())
		return false;
	fid_t highest = toNumber(c.front());
	fileID.setseed(highest);
	return (highest > 0);
}

static bool
checkpoint_chunkId(deque <string> &c)
{
	c.pop_front();
	if (c.empty())
		return false;
	fid_t highest = toNumber(c.front());
	chunkID.setseed(highest);
	return (highest > 0);
}

static bool
checkpoint_version(deque <string> &c)
{
	c.pop_front();
	if (c.empty())
		return false;
	int version = toNumber(c.front());
	return version == Checkpoint::VERSION;
}

static bool
checkpoint_time(deque <string> &c)
{
	c.pop_front();
	std::cout << "restoring from checkpoint of " << c.front() << '\n';
	return true;
}

static bool
checkpoint_log(deque <string> &c)
{
	c.pop_front();
	string s;
	while (!c.empty()) {
		s += c.front();
		c.pop_front();
		if (!c.empty())
			s += "/";
	}
	std::cout << "log file is " << s << '\n';
	replayer.openlog(s);
	return true;
}

bool
KFS::restore_chunkVersionInc(deque <string> &c)
{
	c.pop_front();
	if (c.empty())
		return false;
	chunkVersionInc = toNumber(c.front());
	return (chunkVersionInc >= 1);
}

static bool
restore_dentry(deque <string> &c)
{
	string name;
	fid_t id, parent;
	c.pop_front();
	bool ok = pop_name(name, "name", c, true);
	ok = pop_fid(id, "id", c, ok);
	ok = pop_fid(parent, "parent", c, ok);
	if (!ok)
		return false;

	MetaDentry *d = new MetaDentry(parent, name, id);
	return (metatree.insert(d) == 0);
}

static bool
restore_fattr(deque <string> &c)
{
	FileType type;
	fid_t fid;
	long long chunkcount;
	struct timeval mtime, ctime, crtime;
	int16_t numReplicas;

	bool ok = pop_type(type, "fattr", c, true);
	ok = pop_fid(fid, "id", c, ok);
	ok = pop_fid(chunkcount, "chunkcount", c, ok);
	ok = pop_short(numReplicas, "numReplicas", c, ok);
	ok = pop_time(mtime, "mtime", c, ok);
	ok = pop_time(ctime, "ctime", c, ok);
	ok = pop_time(crtime, "crtime", c, ok);
	if (!ok)
		return false;

	MetaFattr *f = new MetaFattr(type, fid, mtime, ctime, crtime, 
					chunkcount, numReplicas);
	return (metatree.insert(f) == 0);
}

static bool
restore_chunkinfo(deque <string> &c)
{
	fid_t fid;
	chunkId_t cid;
	off_t offset;
	seq_t chunkVersion;

	c.pop_front();
	bool ok = pop_fid(fid, "fid", c, true);
	ok = pop_fid(cid, "chunkid", c, ok);
	ok = pop_offset(offset, "offset", c, ok);
	ok = pop_fid(chunkVersion, "chunkVersion", c, ok);
	if (!ok)
		return false;

	MetaChunkInfo *ch = new MetaChunkInfo(fid, offset, cid, chunkVersion);
	if (metatree.insert(ch) == 0) {
		gLayoutManager.AddChunkToServerMapping(cid, fid, NULL);
		return true;
	}
	return false;
}

static void
init_map(DiskEntry &e)
{
	e.add_parser("checkpoint", checkpoint_seq);
	e.add_parser("version", checkpoint_version);
	e.add_parser("fid", checkpoint_fid);
	e.add_parser("chunkId", checkpoint_chunkId);
	e.add_parser("time", checkpoint_time);
	e.add_parser("log", checkpoint_log);
	e.add_parser("chunkVersionInc", restore_chunkVersionInc);
	e.add_parser("dentry", restore_dentry);
	e.add_parser("fattr", restore_fattr);
	e.add_parser("chunkinfo", restore_chunkinfo);
}

/*!
 * \brief rebuild metadata tree from CP file cpname
 * \param[in] cpname	the CP file
 * \return		true if successful
 */
bool
Restorer::rebuild(const string cpname)
{
	const int MAXLINE = 400;
	char line[MAXLINE];
	int lineno = 0;

	DiskEntry entrymap;
	init_map(entrymap);

	file.open(cpname.c_str());
	bool is_ok = !file.fail();

	while (is_ok && !file.eof()) {
		++lineno;
		file.getline(line, MAXLINE);
		is_ok = entrymap.parse(line);
		if (!is_ok)
			std::cerr << "Error at line " << lineno << ": "
					<< line << '\n';
	}

	file.close();
	return is_ok;
}