/**
 * package.h
 * @author wherewindblow
 * @date   May 26, 2018
 */

#pragma once

#include <cstddef>
#include <map>
#include <mutex>

#include <lights/env.h>
#include <lights/sequence.h>
#include <protocol/message_declare.h>
#include <crypto/aes.h>

#include "basics.h"
#include "exception.h"


namespace spaceless {

/**
 * Network package header include some common data of each package.
 */
struct PackageHeader
{
	/**
	 * It's same for all package header with difference version.
	 * @note Cannot add new field.
	 */
	struct Base
	{
		// Version of protocol.
		short version;
		// Indicates how of use content.
		int command;
		// The length of content. If content have padding will not include it.
		int content_length;
	} LIGHTS_NOT_MEMORY_ALIGNMENT;

	/**
	 * To extend package header, only can add new field in the end of extend structure.
	 * And must increase PACKAGE_VERSION after extend package header.
	 * @note Only can add new field in this structure.
	 */
	struct Extend
	{
		// Receives side will transfer as trigger_package_id when send back message.
		int self_package_id;
		// self_package_id of request.
		int trigger_package_id;
	} LIGHTS_NOT_MEMORY_ALIGNMENT;


	// Base structure.
	Base base;

	// Extend structure.
	Extend extend;

	void reset()
	{
		std::memset(this, 0, sizeof(*this));
		base.version = PACKAGE_VERSION;
	}
} LIGHTS_NOT_MEMORY_ALIGNMENT;


struct PackageTriggerSource
{
	PackageTriggerSource(int command = 0, int package_id = 0):
		command(command),
		package_id(package_id)
	{}

	int command;
	int package_id;
};


/**
 * Network package buffer include header and content.
 */
class PackageBuffer
{
public:
	static const std::size_t HEADER_LEN = sizeof(PackageHeader);
	// The length can hold any build-in message that before open connection to avoid expand buffer at first time.
	static const std::size_t STACK_CONTENT_LEN = 320;
	static const std::size_t STACK_BUFFER_LEN = HEADER_LEN + STACK_CONTENT_LEN;
	static const std::size_t FIRST_HEAP_BUFFER_LEN = 512;
	static const std::size_t MAX_BUFFER_LEN = 65536;
	static const std::size_t MAX_CONTENT_LEN = MAX_BUFFER_LEN - HEADER_LEN;

	/**
	 * Creates the PackageBuffer.
	 */
	PackageBuffer() :
		m_buffer(),
		m_data(m_buffer),
		m_length(sizeof(m_buffer))
	{
		header().base.version = PACKAGE_VERSION;
	}

	/**
	 * Disable copy constructor.
	 */
	PackageBuffer(const PackageBuffer&) = delete;

	/**
	 * Returns package header.
	 */
	PackageHeader& header()
	{
		return *reinterpret_cast<PackageHeader*>(m_data);
	}

	/**
	 * Returns package header.
	 */
	const PackageHeader& header() const
	{
		return *reinterpret_cast<const PackageHeader*>(m_data);
	}

	/**
	 * Returns package content.
	 */
	lights::Sequence content()
	{
		return {m_data + HEADER_LEN, static_cast<std::size_t>(header().base.content_length)};
	}

	/**
	 * Returns package content.
	 */
	lights::SequenceView content() const
	{
		return {m_data + HEADER_LEN, static_cast<std::size_t>(header().base.content_length)};
	}

	/**
	 * Return package content buffer.
	 * @note After this operation, any pointer to internal buffer will be invalidate.
	 */
	lights::Sequence content_buffer(std::size_t expect_length = 0)
	{
		expect_content_length(expect_length);
		return {m_data + HEADER_LEN, m_length - HEADER_LEN};
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	char* data()
	{
		return m_data;
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	const char* data() const
	{
		return m_data;
	}

	/**
	 * Returns header and content length.
	 */
	std::size_t valid_length() const
	{
		return HEADER_LEN + header().base.content_length;
	}

	/**
	 * Expects content length.
	 * @note After this operation, any pointer to internal buffer will be invalidate.
	 */
	bool expect_content_length(std::size_t content_length);

private:
	char m_buffer[STACK_BUFFER_LEN];
	char* m_data;
	std::size_t m_length;
};


/**
 * Network package include header and content. It's use to optimize package memory using.
 * It's light object, so can pass by value.
 */
class Package
{
public:
	static const std::size_t HEADER_LEN = sizeof(PackageHeader);

	using LengthCalculator = std::size_t (*)(std::size_t);

	struct Entry
	{
		Entry(int id, std::size_t length, char* data):
			id(id),
			length(length),
			data(data),
			length_calculator()
		{}

		int id;
		std::size_t length;
		char* data;
		LengthCalculator length_calculator;
	};

	/**
	 * Creates the PackageBuffer.
	 */
	Package(Entry* entry = nullptr) :
		m_entry(entry)
	{}

	/**
	 * Checks package is valid.
	 */
	bool is_valid() const
	{
		return m_entry != nullptr;
	}

	/**
	 * Returns package id.
	 */
	int package_id() const
	{
		return m_entry->id;
	}

	/**
	 * Sets length calculator to calculate content length.
	 */
	void set_calculate_length(LengthCalculator length_calculator)
	{
		m_entry->length_calculator = length_calculator;
	}

	/**
	 * Returns package header.
	 */
	PackageHeader& header()
	{
		return *reinterpret_cast<PackageHeader*>(m_entry->data);
	}

	/**
	 * Returns package header.
	 */
	const PackageHeader& header() const
	{
		return *reinterpret_cast<const PackageHeader*>(m_entry->data);
	}

	/**
	 * Returns buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::Sequence content()
	{
		return {m_entry->data + HEADER_LEN, static_cast<std::size_t>(header().base.content_length)};
	}

	/**
	 * Returns buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::SequenceView content() const
	{
		return {m_entry->data + HEADER_LEN, static_cast<std::size_t>(header().base.content_length)};
	}

	/**
	 * Returns all content buffer.
	 */
	lights::Sequence content_buffer()
	{
		return {m_entry->data + HEADER_LEN, m_entry->length - HEADER_LEN};
	}

	/**
	 * Returns all content buffer.
	 */
	lights::SequenceView content_buffer() const
	{
		return {m_entry->data + HEADER_LEN, m_entry->length - HEADER_LEN};
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	char* data()
	{
		return m_entry->data;
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	const char* data() const
	{
		return m_entry->data;
	}

	/**
	 * Returns header and content length according is cipher.
	 */
	std::size_t valid_length() const
	{
		int content_len = header().base.content_length;
		if (m_entry->length_calculator)
		{
			return HEADER_LEN + m_entry->length_calculator(static_cast<size_t>(content_len));
		}
		else
		{
			return HEADER_LEN + content_len;
		}
	}

	/**
	 * Returns underlying buffer length.
	 */
	std::size_t buffer_length() const
	{
		return m_entry->length;
	}

	/**
	 * Parses package content as protocol message.
	 */
	void parse_to_protocol(protocol::Message& msg) const;

	/**
	 * Gets trigger source that is necessary for send back package.
	 */
	PackageTriggerSource get_trigger_source() const
	{
		return PackageTriggerSource(header().base.command, header().extend.self_package_id);
	}

private:
	Entry* m_entry;
};


/**
 * Manager of package and guarantee package are valid when connection underlying write is call.
 * All operation in this class is thread safe.
 */
class PackageManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(PackageManager);

	/**
	 * Registers package buffer.
	 * @throw Throws exception if register failure.
	 */
	Package register_package(int content_len);

	/**
	 * Removes package buffer.
	 */
	void remove_package(int package_id);

	/**
	 * Finds package buffer.
	 * @note Returns nullptr if cannot find package.
	 */
	Package find_package(int package_id);

	/**
	 * Gets package buffer.
	 * @throw Throws exception if cannot find package.
	 */
	Package get_package(int package_id);

	/**
	 * Returns number of package.
	 */
	std::size_t size();

private:
	int m_next_id = 1;
	std::map<int, Package::Entry> m_package_list;
	std::mutex m_mutex;
};


} // namespace spaceless
