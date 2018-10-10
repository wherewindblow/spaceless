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

#include "basics.h"
#include "exception.h"


namespace spaceless {

/**
 * Network package header include some common data of each package.
 */
struct PackageHeader
{
	// Version of protocol.
	short version;
	// Indicates how of use content.
	int command;
	// Recieves side will transfer as trigger_trans_id when send back message.
	int self_trans_id;
	// self_trans_id of request.
	int trigger_trans_id;
	// The length of content.
	int content_length;
} LIGHTS_NOT_MEMEORY_ALIGNMENT;


struct PackageTriggerSource
{
	int command;
	int self_trans_id;
};


/**
 * Network package buffer include header and content.
 */
class PackageBuffer
{
public:
	static const std::size_t MAX_BUFFER_LEN = 65536;
	static const std::size_t MAX_HEADER_LEN = sizeof(PackageHeader);
	static const std::size_t MAX_CONTENT_LEN = MAX_BUFFER_LEN - MAX_HEADER_LEN;

	/**
	 * Creates the PackageBuffer.
	 */
	PackageBuffer(int package_id) :
		m_id(package_id)
	{
		header().version = PROTOCOL_PACKAGE_VERSION;
	}

	int package_id() const
	{
		return m_id;
	}

	/**
	 * Returns package header of buffer.
	 */
	PackageHeader& header()
	{
		return *reinterpret_cast<PackageHeader*>(m_buffer);
	}

	/**
	 * Returns package header of buffer.
	 */
	const PackageHeader& header() const
	{
		return *reinterpret_cast<const PackageHeader*>(m_buffer);
	}

	/**
	 * Returns buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::Sequence content()
	{
		return {m_buffer + MAX_HEADER_LEN, static_cast<std::size_t>(header().content_length)};
	}

	/**
	 * Returns buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::SequenceView content() const
	{
		return {m_buffer + MAX_HEADER_LEN, static_cast<std::size_t>(header().content_length)};
	}

	/**
	 * Returns all content buffer.
	 */
	lights::Sequence content_buffer()
	{
		return {m_buffer + MAX_HEADER_LEN, MAX_CONTENT_LEN};
	}

	/**
	 * Returns all content buffer.
	 */
	lights::SequenceView content_buffer() const
	{
		return {m_buffer + MAX_HEADER_LEN, MAX_CONTENT_LEN};
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	char* data()
	{
		return m_buffer;
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	const char* data() const
	{
		return m_buffer;
	}

	/**
	 * Returns header and content length.
	 */
	std::size_t total_length() const
	{
		return PackageBuffer::MAX_HEADER_LEN + header().content_length;
	}

	/**
	 * Parses package content as protocol message.
	 */
	void parse_as_protocol(protocol::Message& msg) const;

	PackageTriggerSource get_trigger_source() const
	{
		return { header().command, header().self_trans_id };
	}

	void operator=(const PackageBuffer& rhs)
	{
		int id = m_id;
		lights::copy_array(m_buffer, rhs.m_buffer);
		m_id = id;
	}

private:
	int m_id;
	char m_buffer[MAX_BUFFER_LEN];
};


/**
 * Manager of package buffer and guarantee package are valid when connetion underlying write is call.
 */
class PackageBufferManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(PackageBufferManager);

	/**
	 * Registers package buffer.
	 * @throw Throws exception if register failure.
	 */
	PackageBuffer& register_package();

	/**
	 * Removes package buffer.
	 */
	void remove_package(int package_id);

	/**
	 * Finds package buffer.
	 * @note Returns nullptr if cannot find package.
	 */
	PackageBuffer* find_package(int package_id);

	/**
	 * Gets package buffer.
	 * @throw Throws exception if cannot find package.
	 */
	PackageBuffer& get_package(int package_id);

private:
	int m_next_id = 1;
	std::map<int, PackageBuffer> m_package_list;
	std::mutex m_mutex;
};


} // namespace spaceless
