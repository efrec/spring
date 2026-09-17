/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <deque>
#include <vector>
#include "Command.h"

class CUnit;

/// A wrapper class for std::deque<Command> to keep track of commands
class CCommandQueue {

	friend class CCommandAI;
	friend class CFactoryCAI;

	// see CommandAI.cpp for further creg stuff for this class
	CR_DECLARE_STRUCT(CCommandQueue)

	public:
		enum QueueType {
			OrderQueueType,
			RallyQueueType,
			BuildQueueType
		};

		inline QueueType GetType() const { return queueType; }

		/// one recorded mutation, raised to Lua by FlushEvents
		struct Event {
			int index;
			int tag;
			int cmdID;
			bool added;
		};

	public:
		/// limit to a float's integer range
		static const int maxTagValue = (1 << 24); // 16777216

		typedef std::deque<Command> basis;

		typedef basis::size_type              size_type;
		typedef basis::iterator               iterator;
		typedef basis::const_iterator         const_iterator;
		typedef basis::reverse_iterator       reverse_iterator;
		typedef basis::const_reverse_iterator const_reverse_iterator;

		inline bool empty() const { return queue.empty(); }

		inline size_type size() const { return queue.size(); }

		// Mutators only record. Nothing runs Lua until CUnitHandler::FlushQueueEvents calls FlushEvents.
		void push_back(const Command& cmd);
		void push_front(const Command& cmd);

		void emplace_back(Command&& cmd);
		void emplace_front(Command&& cmd);

		iterator insert(iterator pos, const Command& cmd);

		void pop_back();
		void pop_front();

		iterator erase(iterator pos);
		iterator erase(iterator first, iterator last);

		void clear();

		/// overwrite a queued command in place and keep the cmd's tag
		void Replace(size_type idx, const Command& cmd);

		/// raise any recorded queue events, then UnitQueueChanged, then UnitCommandActivated
		// if the front command updated; events recorded during these raise on the next flush
		void FlushEvents();

		inline iterator       end()         { return queue.end(); }
		inline const_iterator end()   const { return queue.end(); }
		inline iterator       begin()       { return queue.begin(); }
		inline const_iterator begin() const { return queue.begin(); }

		inline reverse_iterator       rend()         { return queue.rend(); }
		inline const_reverse_iterator rend()   const { return queue.rend(); }
		inline reverse_iterator       rbegin()       { return queue.rbegin(); }
		inline const_reverse_iterator rbegin() const { return queue.rbegin(); }

		inline       Command& back()        { return queue.back(); }
		inline const Command& back()  const { return queue.back(); }
		inline       Command& front()       { return queue.front(); }
		inline const Command& front() const { return queue.front(); }

		inline       Command& at(size_type i)       { return queue.at(i); }
		inline const Command& at(size_type i) const { return queue.at(i); }

		inline       Command& operator[](size_type i)       { return queue[i]; }
		inline const Command& operator[](size_type i) const { return queue[i]; }

	private:
		CCommandQueue() : queueType(OrderQueueType), tagCounter(0), owner(nullptr), flushedFrontTag(-1), flushing(false) {};
		CCommandQueue(const CCommandQueue&);
		CCommandQueue& operator=(const CCommandQueue&);

	private:
		inline int GetNextTag();
		inline void SetQueueType(QueueType type) { queueType = type; }
		inline void SetOwner(CUnit* unit) { owner = unit; }

		bool WantsEvents() const;
		void Record(bool added, size_type idx, const Command& cmd);

	private:
		std::deque<Command> queue;
		QueueType queueType;
		int tagCounter;
		CUnit* owner;

		std::vector<Event> pending;
		int flushedFrontTag;
		bool flushing;
};


inline int CCommandQueue::GetNextTag()
{
	tagCounter++;
	if (tagCounter >= maxTagValue)
		tagCounter = 1;

	return tagCounter;
}
