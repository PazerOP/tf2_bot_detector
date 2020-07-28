#pragma once

#include "Clock.h"
#include "ISetupFlowPage.h"

#include <srcon/async_client.h>

#include <array>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace tf2_bot_detector
{
	class TF2CommandLinePage final : public ISetupFlowPage
	{
	public:
		bool ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;

		void Init(const Settings& settings) override;
		bool CanCommit() const override { return true; }
		void Commit(Settings& settings) override;

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

	private:
		static constexpr duration_t CL_UPDATE_INTERVAL = std::chrono::seconds(1);

		struct TF2CommandLine
		{
			static TF2CommandLine Parse(const std::string_view& cmdLine);

			std::string m_FullCommandLine;

			bool m_UseRCON = false;
			std::string m_IP;
			std::string m_RCONPassword;
			std::optional<uint16_t> m_RCONPort;

			bool IsPopulated() const;
		};

		void DrawAutoLaunchTF2Checkbox(const DrawState& ds);
		void DrawLaunchTF2Button(const DrawState& ds);
		void DrawCommandLineArgsInvalid(const DrawState& ds, const TF2CommandLine& args);

		struct RCONClientData
		{
			RCONClientData(std::string pwd, uint16_t port);
			std::unique_ptr<srcon::async_client> m_Client;
			std::shared_future<std::string> m_Future;

			[[nodiscard]] bool Update();

		private:
			bool m_Success = false;
			std::array<float, 4> m_MessageColor{ 1, 1, 1, 1 };
			std::string m_Message;
		};

		// We don't want the user to get stuck in an annoying loop of
		// tf2 auto-relaunching the moment they close the game.
		bool m_IsAutoLaunchAllowed = true;

		struct Data
		{
			time_point_t m_LastTF2LaunchTime{};

			bool m_MultipleInstances = false;
			std::optional<TF2CommandLine> m_CommandLineArgs;
			std::shared_future<std::vector<std::string>> m_CommandLineArgsFuture;
			bool m_AtLeastOneUpdateRun = false;

			time_point_t m_LastCLUpdate{};

			std::string m_RandomRCONPassword;
			uint16_t m_RandomRCONPort;
			bool m_RCONSuccess = false;
			std::optional<RCONClientData> m_TestRCONClient;

			void TryUpdateCmdlineArgs();

		} m_Data;
	};
}
