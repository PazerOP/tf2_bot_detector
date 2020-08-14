#include "Log.h"
#include "Tests.h"

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_NOSTDOUT

#include <catch2/catch.hpp>
#include <mh/text/format.hpp>

namespace Catch
{
	enum class LogBufType
	{
		Out,
		Log,
		Error,
	};
	struct LogBuf final : std::stringbuf
	{
		LogBuf(LogBufType type) : m_Type(type)
		{
		}
		~LogBuf()
		{
			pubsync();
		}

		int sync() override
		{
			auto message = mh::format("[Catch2] {}", str());
			switch (m_Type)
			{
			case LogBufType::Out:
			case LogBufType::Log:
				tf2_bot_detector::Log(std::move(message));
				break;

			default:
				tf2_bot_detector::LogError(MH_SOURCE_LOCATION_CURRENT(),
					mh::format("[Catch2] Unknown LogBufType {}", int(m_Type)));
				[[fallthrough]];
			case LogBufType::Error:
				tf2_bot_detector::LogError(std::move(message));
				break;
			}

			str(""); // Clear out the buffer
			return 0;
		}

		LogBufType m_Type;
	};

	std::ostream& cout()
	{
		static std::ostream s_Stream(new LogBuf(LogBufType::Out));
		return s_Stream;
	}

	std::ostream& clog()
	{
		static std::ostream s_Stream(new LogBuf(LogBufType::Log));
		return s_Stream;
	}

	std::ostream& cerr()
	{
		static std::ostream s_Stream(new LogBuf(LogBufType::Error));
		return s_Stream;
	}
}

int tf2_bot_detector::RunTests()
{
	return Catch::Session().run();
}
