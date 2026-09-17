/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "CommandQueue.h"

#include <cassert>

#include "Sim/Units/Unit.h"
#include "System/EventHandler.h"


bool CCommandQueue::WantsEvents() const
{
	assert(owner != nullptr);
	return eventHandler.HasQueueEventClients();
}

void CCommandQueue::Record(bool added, size_type idx, const Command& cmd)
{
	if (!WantsEvents())
		return;

	pending.push_back({static_cast<int>(idx), static_cast<int>(cmd.GetTag()), cmd.GetID(), added});
}


void CCommandQueue::FlushEvents()
{
	assert(owner != nullptr);

	if (flushing)
		return;

	flushing = true;

	// by index and by count: a callin may append to pending
	// those entries stay behind for the next flush in-order
	const size_t count = pending.size();

	for (size_t i = 0; i < count; i++) {
		const Event e = pending[i];

		if (e.added) {
			eventHandler.UnitCommandAdded(owner, queueType, e.index, e.tag, e.cmdID);
		} else {
			eventHandler.UnitCommandRemoved(owner, queueType, e.index, e.tag, e.cmdID);
		}
	}

	pending.erase(pending.begin(), pending.begin() + count);

	if (count > 0)
		eventHandler.UnitQueueChanged(owner, queueType);

	if (eventHandler.HasCommandActivatedClients()) {
		const int frontTag = queue.empty()? -1: static_cast<int>(queue.front().GetTag());

		if (frontTag != flushedFrontTag) {
			flushedFrontTag = frontTag;

			if (frontTag != -1)
				eventHandler.UnitCommandActivated(owner, queueType, frontTag, queue.front().GetID());
		}
	}

	flushing = false;
}


void CCommandQueue::push_back(const Command& cmd)
{
	queue.push_back(cmd);
	queue.back().SetTag(GetNextTag());

	Record(true, queue.size() - 1, queue.back());
}

void CCommandQueue::push_front(const Command& cmd)
{
	queue.push_front(cmd);
	queue.front().SetTag(GetNextTag());

	Record(true, 0, queue.front());
}


void CCommandQueue::emplace_back(Command&& cmd)
{
	queue.emplace_back(cmd);
	queue.back().SetTag(GetNextTag());

	Record(true, queue.size() - 1, queue.back());
}

void CCommandQueue::emplace_front(Command&& cmd)
{
	queue.emplace_front(cmd);
	queue.front().SetTag(GetNextTag());

	Record(true, 0, queue.front());
}


CCommandQueue::iterator CCommandQueue::insert(iterator pos, const Command& cmd)
{
	Command tmpCmd = cmd;
	tmpCmd.SetTag(GetNextTag());

	const size_type idx = static_cast<size_type>(pos - queue.begin());
	const iterator it = queue.insert(pos, tmpCmd);

	Record(true, idx, *it);
	return it;
}


void CCommandQueue::pop_back()
{
	if (queue.empty())
		return;

	Record(false, queue.size() - 1, queue.back());
	queue.pop_back();
}

void CCommandQueue::pop_front()
{
	if (queue.empty())
		return;

	Record(false, 0, queue.front());
	queue.pop_front();
}


CCommandQueue::iterator CCommandQueue::erase(iterator pos)
{
	Record(false, static_cast<size_type>(pos - queue.begin()), *pos);
	return queue.erase(pos);
}

CCommandQueue::iterator CCommandQueue::erase(iterator first, iterator last)
{
	for (iterator it = last; it != first; ) {
		--it;
		Record(false, static_cast<size_type>(it - queue.begin()), *it);
	}

	return queue.erase(first, last);
}


void CCommandQueue::Replace(size_type idx, const Command& cmd)
{
	Record(false, idx, queue[idx]);
	queue[idx] = cmd;
	Record(true, idx, queue[idx]);
}


void CCommandQueue::clear()
{
	for (size_type i = queue.size(); i > 0; --i)
		Record(false, i - 1, queue[i - 1]);

	queue.clear();
}
