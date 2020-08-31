#include "UpdateManager.h"
#include "Config/Settings.h"
#include "Networking/GithubAPI.h"
#include "Networking/HTTPClient.h"
#include "Networking/HTTPHelpers.h"
#include "Platform/Platform.h"
#include "Util/JSONUtils.h"
#include "Log.h"
#include "ReleaseChannel.h"

#include <mh/algorithm/multi_compare.hpp>
#include <mh/future.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/types/disable_copy_move.hpp>
#include <mh/variant.hpp>
#include <nlohmann/json.hpp>

#include <compare>
#include <variant>

using namespace std::string_view_literals;
using namespace tf2_bot_detector;

namespace tf2_bot_detector
{
	void to_json(nlohmann::json& j, const Platform::OS& d)
	{
		j = mh::find_enum_value_name(d);
	}
	void from_json(const nlohmann::json& j, Platform::OS& d)
	{
		mh::find_enum_value(j, d);
	}

	void to_json(nlohmann::json& j, const Platform::Arch& d)
	{
		j = mh::find_enum_value_name(d);
	}
	void from_json(const nlohmann::json& j, Platform::Arch& d)
	{
		mh::find_enum_value(j, d);
	}

	void to_json(nlohmann::json& j, const BuildInfo::BuildVariant& d)
	{
		j =
		{
			{ "os", d.m_OS },
			{ "arch", d.m_Arch },
			{ "download_url", d.m_DownloadURL },
		};
	}
	void from_json(const nlohmann::json& j, BuildInfo::BuildVariant& d)
	{
		d.m_OS = j.at("os");
		d.m_Arch = j.at("arch");
		d.m_DownloadURL = j.at("download_url");
	}

	void to_json(nlohmann::json& j, const BuildInfo& d)
	{
		j =
		{
			{ "version", d.m_Version },
			{ "build_type", d.m_ReleaseChannel },
			{ "github_url", d.m_GitHubURL },
			{ "msix_bundle_url", d.m_MSIXBundleURL },
			{ "updater", d.m_Updater },
			{ "portable", d.m_Portable },
		};
	}
	void from_json(const nlohmann::json& j, BuildInfo& d)
	{
		d.m_Version = j.at("version");
		d.m_ReleaseChannel = j.at("build_type");
		try_get_to_defaulted(j, d.m_GitHubURL, "github_url");
		try_get_to_defaulted(j, d.m_MSIXBundleURL, "msix_bundle_url");
		j.at("updater").get_to(d.m_Updater);
		j.at("portable").get_to(d.m_Portable);
	}
}

namespace
{
	class UpdateManager final : public IUpdateManager
	{
		struct AvailableUpdate final : IAvailableUpdate
		{
			AvailableUpdate(UpdateManager& parent, BuildInfo&& buildInfo);

			bool CanSelfUpdate() const override;
			void BeginSelfUpdate() const override;

			UpdateManager& m_Parent;
		};

	public:
		UpdateManager(const Settings& settings);

		void Update() override;
		UpdateStatus GetUpdateStatus() const override;
		std::string GetErrorString() const override;
		const AvailableUpdate* GetAvailableUpdate() const override;

		void QueueUpdateCheck() override { m_IsUpdateQueued = true; }

	private:
		const Settings& m_Settings;

		struct BaseExceptionData
		{
			BaseExceptionData(const std::type_info& type, std::string message, const std::exception_ptr& exception);

			const std::type_info& m_Type;
			std::string m_Message;
			std::exception_ptr m_Exception;
		};

		template<typename T>
		struct ExceptionData : BaseExceptionData
		{
			using BaseExceptionData::BaseExceptionData;
		};

		struct DownloadedUpdateTool
		{
			std::filesystem::path m_Path;
			std::string m_Arguments;
		};

		struct UpdateToolResult
		{
			bool m_Success{};
		};
		struct UpdateToolExceptionData : BaseExceptionData
		{
			using BaseExceptionData::BaseExceptionData;
		};

		using UpdateCheckState_t = std::variant<std::future<BuildInfo>, AvailableUpdate, ExceptionData<BuildInfo>>;
		UpdateCheckState_t m_UpdateCheckState;

		using State_t = std::variant<
			std::monostate,

			std::future<InstallUpdate::Result>,
			InstallUpdate::Result,
			ExceptionData<InstallUpdate::Result>,

			std::future<DownloadedUpdateTool>,
			DownloadedUpdateTool,
			ExceptionData<DownloadedUpdateTool>,

			std::future<UpdateToolResult>,
			UpdateToolResult,
			ExceptionData<UpdateToolResult>

		>;
		State_t m_State;

		template<typename TFutureResult> bool UpdateFutureState();

		bool CanReplaceUpdateCheckState() const;

		bool m_IsUpdateQueued = true;
		bool m_IsInstalled;
	};

	UpdateManager::UpdateManager(const Settings& settings) :
		m_Settings(settings),
		m_IsInstalled(Platform::IsInstalled())
	{
	}

	template<typename TFutureResult>
	bool UpdateManager::UpdateFutureState()
	{
		if (auto future = std::get_if<std::future<TFutureResult>>(&m_State))
		{
			try
			{
				if (mh::is_future_ready(*future))
					m_State.emplace<TFutureResult>(future->get());
			}
			catch (const std::exception& e)
			{
				LogException(MH_SOURCE_LOCATION_CURRENT(), e, __FUNCSIG__);
				m_State.emplace<ExceptionData<TFutureResult>>(typeid(e), e.what(), std::current_exception());
			}

			return true;
		}

		return false;
	}

	void UpdateManager::Update()
	{
		if (m_IsUpdateQueued && CanReplaceUpdateCheckState())
		{
			auto client = m_Settings.GetHTTPClient();
			const auto releaseChannel = m_Settings.m_ReleaseChannel.value_or(ReleaseChannel::None);
			if (client && (releaseChannel != ReleaseChannel::None))
			{
				auto sharedClient = client->shared_from_this();
				m_UpdateCheckState = std::async([sharedClient, releaseChannel]() -> BuildInfo
					{
						const mh::fmtstr<256> url(
							"https://tf2bd-util.pazer.us/AppInstaller/LatestVersion.json?type={:v}",
							mh::enum_fmt(releaseChannel));

						DebugLog("HTTP GET {}", url);
						auto response = sharedClient->GetString(url.view());

						auto json = nlohmann::json::parse(response);

						return json.get<BuildInfo>();
					});

				m_IsUpdateQueued = false;
			}
		}

		if (auto future = std::get_if<std::future<BuildInfo>>(&m_UpdateCheckState))
		{
			try
			{
				if (mh::is_future_ready(*future))
					m_UpdateCheckState.emplace<AvailableUpdate>(*this, future->get());
			}
			catch (const std::exception& e)
			{
				LogException(MH_SOURCE_LOCATION_CURRENT(), e, __FUNCSIG__);
				m_UpdateCheckState.emplace<ExceptionData<BuildInfo>>(typeid(e), e.what(), std::current_exception());
			}
		}

		UpdateFutureState<InstallUpdate::Result>() ||
			UpdateFutureState<DownloadedUpdateTool>() ||
			UpdateFutureState<UpdateToolResult>();
	}

	UpdateStatus UpdateManager::GetUpdateStatus() const
	{
		if (m_IsUpdateQueued)
		{
			if (m_Settings.m_ReleaseChannel.value_or(ReleaseChannel::None) == ReleaseChannel::None)
				return UpdateStatus::UpdateCheckDisabled;
			else if (!m_Settings.GetHTTPClient())
				return UpdateStatus::InternetAccessDisabled;
			else
				return UpdateStatus::CheckQueued;
		}

		switch (m_State.index())
		{
		case mh::variant_type_index_v<State_t, std::monostate>:
			break;

			//////////////////////////////
			/// InstallUpdate::Result ///
			//////////////////////////////
		case mh::variant_type_index_v<State_t, std::future<InstallUpdate::Result>>:
			return UpdateStatus::Updating;
		case mh::variant_type_index_v<State_t, ExceptionData<InstallUpdate::Result>>:
			return UpdateStatus::UpdateFailed;
		case mh::variant_type_index_v<State_t, InstallUpdate::Result>:
		{
			auto& result = std::get<InstallUpdate::Result>(m_State);
			switch (result.index())
			{
			case mh::variant_type_index_v<InstallUpdate::Result, InstallUpdate::Success>:
				return UpdateStatus::UpdateSuccess;

			default:
				LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown InstallUpdate::Result index {}", result.index());
			case mh::variant_type_index_v<InstallUpdate::Result, InstallUpdate::StartedNoFeedback>:
				return UpdateStatus::Updating;
			case mh::variant_type_index_v<InstallUpdate::Result, InstallUpdate::NeedsUpdateTool>:
				return UpdateStatus::UpdateToolDownloading;
			}
		}

			////////////////////////////
			/// DownloadedUpdateTool ///
			////////////////////////////
		case mh::variant_type_index_v<State_t, std::future<DownloadedUpdateTool>>:
			return UpdateStatus::UpdateToolDownloading;
		case mh::variant_type_index_v<State_t, ExceptionData<DownloadedUpdateTool>>:
			return UpdateStatus::UpdateToolDownloadingFailed;
		case mh::variant_type_index_v<State_t, DownloadedUpdateTool>:
			return UpdateStatus::Updating;

			////////////////////////
			/// UpdateToolResult ///
			////////////////////////
		case mh::variant_type_index_v<State_t, std::future<UpdateToolResult>>:
			return UpdateStatus::Updating;
		case mh::variant_type_index_v<State_t, ExceptionData<UpdateToolResult>>:
			return UpdateStatus::UpdateFailed;
		case mh::variant_type_index_v<State_t, UpdateToolResult>:
		{
			auto& result = std::get<UpdateToolResult>(m_State);
			return result.m_Success ? UpdateStatus::UpdateSuccess : UpdateStatus::UpdateFailed;
		}
		}

		switch (m_UpdateCheckState.index())
		{
		case mh::variant_type_index_v<UpdateCheckState_t, std::future<BuildInfo>>:
			return UpdateStatus::Checking;
		case mh::variant_type_index_v<UpdateCheckState_t, ExceptionData<BuildInfo>>:
			return UpdateStatus::CheckFailed;
		case mh::variant_type_index_v<UpdateCheckState_t, AvailableUpdate>:
		{
			auto& update = std::get<AvailableUpdate>(m_UpdateCheckState);
			return update.m_State.m_Version > VERSION ? UpdateStatus::UpdateAvailable : UpdateStatus::UpToDate;
		}
		}

		LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown update state: m_UpdateCheckState = {}, m_State = {}",
			m_UpdateCheckState.index(), m_State.index());
		return UpdateStatus::Unknown;
	}

	std::string UpdateManager::GetErrorString() const
	{
		return std::visit([](const auto& value) -> std::string
			{
				using type_t = std::decay_t<decltype(value)>;
				if constexpr (std::is_base_of_v<BaseExceptionData, type_t>)
				{
					const BaseExceptionData& d = value;
					return mh::format("{}: {}"sv, d.m_Type.name(), d.m_Message);
				}
				else
				{
					return {};
				}

			}, m_State);
	}

	auto UpdateManager::GetAvailableUpdate() const -> const AvailableUpdate*
	{
		if (GetUpdateStatus() == UpdateStatus::UpdateAvailable)
			return std::get_if<AvailableUpdate>(&m_UpdateCheckState);

		return nullptr;
	}

	/// <summary>
	/// Is it safe to replace the value of m_State? (make sure we won't block the thread)
	/// </summary>
	bool UpdateManager::CanReplaceUpdateCheckState() const
	{
		const auto* future = std::get_if<std::future<BuildInfo>>(&m_UpdateCheckState);
		if (!future)
			return true; // some other value in the variant

		if (!future->valid())
			return true; // future is empty

		if (mh::is_future_ready(*future))
			return true; // future is ready

		return false;
	}

	UpdateManager::AvailableUpdate::AvailableUpdate(UpdateManager& parent, BuildInfo&& buildInfo) :
		IAvailableUpdate(std::move(buildInfo)),
		m_Parent(parent)
	{
	}

	bool UpdateManager::AvailableUpdate::CanSelfUpdate() const
	{
		if (!m_Parent.m_Settings.GetHTTPClient())
			return false;

		if (Platform::IsInstalled())
			return Platform::CanInstallUpdate(m_State);

		// If we're portable, we just need to pass some arguments
		static const bool s_WarnOnce = []
		{
			LogWarning(MH_SOURCE_LOCATION_CURRENT(), "TODO: portable self-update not yet implemented");
			return false;
		}();

		return false;
	}

	void UpdateManager::AvailableUpdate::BeginSelfUpdate() const
	{
		if (!CanSelfUpdate())
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "{} called when CanSelfUpdate() returned false", __func__);
			return;
		}

		auto client = m_Parent.m_Settings.GetHTTPClient();
		if (!client)
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "client was nullptr");
			std::terminate();
		}

		m_Parent.m_State = Platform::BeginInstallUpdate(m_State, *client);
	}

	UpdateManager::BaseExceptionData::BaseExceptionData(const std::type_info& type, std::string message,
		const std::exception_ptr& exception) :
		m_Type(type), m_Message(std::move(message)), m_Exception(exception)
	{
	}
}

std::unique_ptr<IUpdateManager> tf2_bot_detector::IUpdateManager::Create(const Settings& settings)
{
	return std::make_unique<UpdateManager>(settings);
}
