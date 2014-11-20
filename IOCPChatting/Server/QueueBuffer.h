#pragma once

template <int size>
class QueueBuffer
{
public:
	QueueBuffer()
		:itemSize(0)
	{
	}
	~QueueBuffer()
	{
	}

	void Produce(char* const buf, const int len)
	{
		Lock _lock;
		int remainSize = size - itemSize;
		if (remainSize >= len)
		{
			memcpy_s(buffer + itemSize, remainSize, buf, len);
			itemSize += len;
		}
	}

	void Consume(char* const buf, const int len)
	{
		Lock _lock;
		if (itemSize >= len)
		{
			memcpy_s(buf, len, buffer, len);
			memmove_s(buffer, size, buffer + len, itemSize - len);
			itemSize -= len;
		}
	}

	void Peek(char* const buf, const int len)
	{
		if (itemSize >= len)
		{
			memcpy_s(buf, len, buffer, len);
		}
	}

	int GetItemSize() const
	{
		Lock _lock;
		return itemSize;
	}

private:
	char buffer[size];
	int itemSize;
};