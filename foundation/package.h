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
#include "monitor.h"


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
	static const std::size_t BUFFER_LEN = 65536;
	static const std::size_t HEADER_LEN = sizeof(PackageHeader);
	static const std::size_t MAX_CONTENT_LEN = BUFFER_LEN - HEADER_LEN;

	/**
	 * Creates the PackageBuffer.
	 */
	PackageBuffer()
	{
		header().version = PACKAGE_VERSION;
	}

	/**
	 * Returns package header of buffer.
	 */
	PackageHeader& header()
	{
		return *reinterpret_cast<PackageHeader*>(m_buffer);
	}

	/**
	 * Returns content buffer.
	 */
	lights::Sequence content_buffer()
	{
		return {m_buffer + HEADER_LEN, MAX_CONTENT_LEN};
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	char* data()
	{
		return m_buffer;
	}

	char* content_data()
	{
		return m_buffer + HEADER_LEN;
	}

	/**
	 * Returns header and content length.
	 */
	std::size_t total_length() const
	{
		auto header = reinterpret_cast<const PackageHeader*>(m_buffer);
		return PackageBuffer::HEADER_LEN + header->content_length;
	}

private:
	char m_buffer[BUFFER_LEN];
};


/**
 * Network package include header and content. It's use to optimize package memory using.
 * It's light object, so can pass by value.
 */
class Package
{
public:
	static const std::size_t HEADER_LEN = sizeof(PackageHeader);

	Package():
		m_id(0),
		m_data(nullptr),
		m_length(0)
	{}

	/**
	 * Creates the PackageBuffer.
	 */
	Package(int package_id, lights::Sequence data) :
		m_id(package_id),
		m_data(static_cast<char*>(data.data())),
		m_length(data.length())
	{}

	/**
	 * Checks package is valid.
	 */
	bool is_valid() const
	{
		return m_data != nullptr;
	}

	/**
	 * Returns package id.
	 */
	int package_id() const
	{
		return m_id;
	}

	/**
	 * Sets content is cipher.
	 */
	void set_is_cipher(bool is_cipher)
	{
		m_is_cipher = is_cipher;
	}

	/**
	 * Returns is cipher.
	 */
	bool is_cipher() const
	{
		return m_is_cipher;
	}

	/**
	 * Returns package header of buffer.
	 */
	PackageHeader& header()
	{
		return *reinterpret_cast<PackageHeader*>(m_data);
	}

	/**
	 * Returns package header of buffer.
	 */
	const PackageHeader& header() const
	{
		return *reinterpret_cast<const PackageHeader*>(m_data);
	}

	/**
	 * Returns buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::Sequence content()
	{
		return {m_data + HEADER_LEN, static_cast<std::size_t>(header().content_length)};
	}

	/**
	 * Returns buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::SequenceView content() const
	{
		return {m_data + HEADER_LEN, static_cast<std::size_t>(header().content_length)};
	}

	/**
	 * Returns all content buffer.
	 */
	lights::Sequence content_buffer()
	{
		return {m_data + HEADER_LEN, m_length - HEADER_LEN};
	}

	/**
	 * Returns all content buffer.
	 */
	lights::SequenceView content_buffer() const
	{
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
	 * Returns header and content length according is cipher.
	 */
	std::size_t valid_length() const
	{
		if (m_is_cipher)
		{
			return HEADER_LEN + crypto::aes_cipher_length(static_cast<std::size_t>(header().content_length));
		}
		else
		{
			return HEADER_LEN + header().content_length;
		}
	}

	/**
	 * Returns underlying buffer length.
	 */
	std::size_t buffer_length() const
	{
		return m_length;
	}

	/**
	 * Parses package content as protocol message.
	 */
	void parse_to_protocol(protocol::Message& msg) const;

	/**
	 * Gets trigger source that is neccessary for send back package.
	 */
	PackageTriggerSource get_trigger_source() const
	{
		return { header().command, header().self_trans_id };
	}

private:
	int m_id;
	char* m_data;
	std::size_t m_length;
	bool m_is_cipher = false;
};


/**
 * Manager of package and guarantee package are valid when connetion underlying write is call.
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
	std::map<int, lights::Sequence> m_package_list;
	std::mutex m_mutex;
};


} // namespace spaceless
