#include "UpdateManager.h"
#include "Config/Settings.h"
#include "Networking/GithubAPI.h"
#include "Networking/HTTPClient.h"
#include "Networking/HTTPHelpers.h"
#include "Platform/Platform.h"
#include "Util/JSONUtils.h"
#include "Log.h"
#include "ReleaseChannel.h"

#include <libzippp/libzippp.h>
#include <mh/algorithm/multi_compare.hpp>
#include <mh/future.hpp>
#include <mh/raii/scope_exit.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/types/disable_copy_move.hpp>
#include <mh/utility.hpp>
#include <mh/variant.hpp>
#include <nlohmann/json.hpp>

#include <compare>
#include <fstream>
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
		struct UpdateToolResult;

		struct AvailableUpdate final : IAvailableUpdate
		{
			AvailableUpdate(const HTTPClient& client, UpdateManager& parent, BuildInfo&& buildInfo);

			bool CanSelfUpdate() const override;
			bool BeginSelfUpdate() const override;

			const BuildInfo::BuildVariant* m_Updater = nullptr;
			const BuildInfo::BuildVariant* m_Portable = nullptr;

			std::shared_ptr<const HTTPClient> m_HTTPClient;
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

		struct DownloadedBuild
		{
			BuildInfo::BuildVariant m_UpdaterVariant;
			std::filesystem::path m_ExtractedLocation;
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

			std::future<DownloadedBuild>,
			DownloadedBuild,
			ExceptionData<DownloadedBuild>,

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
		template<typename... TFutureResults> bool UpdateFutureStates();

		bool CanReplaceUpdateCheckState() const;

		inline static const std::filesystem::path DOWNLOAD_DIR_ROOT =
			std::filesystem::temp_directory_path() / "TF2 Bot Detector" / "Portable Updates";

		static std::future<DownloadedBuild> DownloadBuild(const HTTPClient& client,
			BuildInfo::BuildVariant tool, BuildInfo::BuildVariant updater);
		static std::future<DownloadedUpdateTool> DownloadUpdateTool(const HTTPClient& client,
			BuildInfo::BuildVariant updater, std::string args);
		static std::future<UpdateToolResult> RunUpdateTool(std::filesystem::path path, std::string args);

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

	template<typename... TFutureResults>
	bool UpdateManager::UpdateFutureStates()
	{
		return (... || UpdateFutureState<TFutureResults>());
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
				auto client = m_Settings.GetHTTPClient();
				if (client)
				{
					if (mh::is_future_ready(*future))
						m_UpdateCheckState.emplace<AvailableUpdate>(*client, *this, future->get());
				}
				else
				{
					LogError(MH_SOURCE_LOCATION_CURRENT(), "HTTPClient unavailable, cancelling update");
					m_UpdateCheckState.emplace<0>();
				}
			}
			catch (const std::exception& e)
			{
				LogException(MH_SOURCE_LOCATION_CURRENT(), e, __FUNCSIG__);
				m_UpdateCheckState.emplace<ExceptionData<BuildInfo>>(typeid(e), e.what(), std::current_exception());
			}
		}

		if (auto downloadedBuild = std::get_if<DownloadedBuild>(&m_State))
		{
			[&]
			{
				auto client = m_Settings.GetHTTPClient();
				if (!client)
				{
					LogError(MH_SOURCE_LOCATION_CURRENT(), "HTTPClient unavailable");
					return;
				}

				m_State = DownloadUpdateTool(*client, downloadedBuild->m_UpdaterVariant,
					mh::format("--update-type Portable --source-path {} --dest-path {}",
						downloadedBuild->m_ExtractedLocation, Platform::GetCurrentExeDir()));
			}();
		}
		else if (auto installUpdateResult = std::get_if<InstallUpdate::Result>(&m_State))
		{
			[&]
			{
				auto needsUpdateTool = std::get_if<InstallUpdate::NeedsUpdateTool>(installUpdateResult);
				if (!needsUpdateTool)
					return;

				auto availableUpdate = GetAvailableUpdate();
				if (!availableUpdate)
				{
					LogError(MH_SOURCE_LOCATION_CURRENT(), "availableUpdate was nullptr");
					return;
				}

				if (!availableUpdate->m_Updater)
				{
					LogError(MH_SOURCE_LOCATION_CURRENT(), "availableUpdate->m_Updater was nullptr");
					return;
				}

				auto client = m_Settings.GetHTTPClient();
				if (!client)
				{
					LogError(MH_SOURCE_LOCATION_CURRENT(), "HTTPClient unavailable");
					return;
				}

				m_State = DownloadUpdateTool(*client, *availableUpdate->m_Updater, needsUpdateTool->m_UpdateToolArgs);
			}();
		}
		else if (auto downloadedUpdateTool = std::get_if<DownloadedUpdateTool>(&m_State))
		{
			m_State = RunUpdateTool(downloadedUpdateTool->m_Path, downloadedUpdateTool->m_Arguments);
		}

		UpdateFutureStates<DownloadedBuild, InstallUpdate::Result, DownloadedUpdateTool, UpdateToolResult>();
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

			///////////////////////
			/// DownloadedBuild ///
			///////////////////////
		case mh::variant_type_index_v<State_t, std::future<DownloadedBuild>>:
			return UpdateStatus::Downloading;
		case mh::variant_type_index_v<State_t, ExceptionData<DownloadedBuild>>:
			return UpdateStatus::DownloadFailed;
		case mh::variant_type_index_v<State_t, DownloadedBuild>:
			return UpdateStatus::DownloadSuccess;

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
				[[fallthrough]];
			case mh::variant_type_index_v<InstallUpdate::Result, InstallUpdate::StartedNoFeedback>:
				return UpdateStatus::Updating;
			case mh::variant_type_index_v<InstallUpdate::Result, InstallUpdate::NeedsUpdateTool>:
				return UpdateStatus::UpdateToolRequired;
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
			return update.m_BuildInfo.m_Version > VERSION ? UpdateStatus::UpdateAvailable : UpdateStatus::UpToDate;
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

	static void SaveFile(const std::filesystem::path& path, const void* dataBegin, const void* dataEnd)
	{
		std::filesystem::create_directories(mh::copy(path).remove_filename());

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.exceptions(std::ios::badbit | std::ios::failbit);
		file.write(reinterpret_cast<const char*>(dataBegin),
			static_cast<const std::byte*>(dataEnd) - static_cast<const std::byte*>(dataBegin));
	}

	static void ExtractArchive(const libzippp::ZipArchive& archive, const std::filesystem::path& directory)
	{
		try
		{
			std::filesystem::create_directories(directory);
		}
		catch (...)
		{
			std::throw_with_nested(std::runtime_error(mh::format("{}: Failed to create directory(s) for {}",
				MH_SOURCE_LOCATION_CURRENT(), directory)));
		}

		try
		{
			for (const auto& entry : archive.getEntries())
			{
				const std::filesystem::path path = directory / entry.getName();
				if (!entry.isFile())
					continue;

				std::filesystem::create_directories(mh::copy(path).remove_filename());

				std::ofstream file(path, std::ios::binary);
				file.exceptions(std::ios::badbit | std::ios::failbit);

				const auto result = entry.readContent(file);
				if (result != LIBZIPPP_OK)
				{
					throw std::runtime_error(mh::format("{}: entry.readContent() returned {}",
						MH_SOURCE_LOCATION_CURRENT(), result));
				}
			}
		}
		catch (...)
		{
			std::throw_with_nested(std::runtime_error(mh::format("{}: Failed to extract archive entries",
				MH_SOURCE_LOCATION_CURRENT())));
		}
	}

	static void DownloadAndExtractZip(const HTTPClient& client, const URL& url,
		const std::filesystem::path& extractDir)
	{
		Log(MH_SOURCE_LOCATION_CURRENT(), "Downloading {}...", url);
		const auto data = client.GetString(url);

		// Need to save to a file due to a libzippp bug in ZipArchive::fromBuffer
		const auto tempZipPath = mh::copy(extractDir).replace_extension(".zip");
		Log(MH_SOURCE_LOCATION_CURRENT(), "Saving zip to {}...", tempZipPath);

		mh::scope_exit scopeExit([&]
			{
				Log(MH_SOURCE_LOCATION_CURRENT(), "Deleting {}...", tempZipPath);
				std::filesystem::remove(tempZipPath);
			});

		SaveFile(tempZipPath, data.data(), data.data() + data.size());

		{
			using namespace libzippp;
			Log(MH_SOURCE_LOCATION_CURRENT(), "Extracting {} to {}...", tempZipPath, extractDir);
			ZipArchive archive(tempZipPath.string());
			archive.open();
			ExtractArchive(archive, extractDir);
		}
	}

	auto UpdateManager::DownloadBuild(const HTTPClient& client,
		BuildInfo::BuildVariant tool, BuildInfo::BuildVariant updater) ->
		std::future<DownloadedBuild>
	{
		const auto downloadDir = DOWNLOAD_DIR_ROOT / mh::format("tool_{}",
			std::chrono::high_resolution_clock::now().time_since_epoch().count());

		auto clientPtr = client.shared_from_this();
		return std::async([clientPtr, tool, updater, downloadDir]() -> DownloadedBuild
			{
				DownloadAndExtractZip(*clientPtr, tool.m_DownloadURL, downloadDir);

				return DownloadedBuild
				{
					.m_UpdaterVariant = updater,
					.m_ExtractedLocation = downloadDir,
				};
			});
	}

	auto UpdateManager::DownloadUpdateTool(const HTTPClient& client, BuildInfo::BuildVariant updater,
		std::string args) -> std::future<DownloadedUpdateTool>
	{
		auto clientPtr = client.shared_from_this();

		return std::async([clientPtr, updater, args]() -> DownloadedUpdateTool
			{
				const auto downloadDir = DOWNLOAD_DIR_ROOT / mh::format("updater_{}",
					std::chrono::high_resolution_clock::now().time_since_epoch().count());

				DownloadAndExtractZip(*clientPtr, updater.m_DownloadURL, downloadDir);

				return DownloadedUpdateTool
				{
					// FIXME linux
					.m_Path = downloadDir / "tf2_bot_detector_updater.exe",
					.m_Arguments = args,
				};
			});
	}

	auto UpdateManager::RunUpdateTool(std::filesystem::path path, std::string args) ->
		std::future<UpdateToolResult>
	{
		// For now, all of our paths through the update tool require that we close ourselves
		args += mh::format(" --wait-pid {}", Platform::Processes::GetCurrentProcessID());

		return std::async([path, args]() -> UpdateToolResult
			{
				Log(MH_SOURCE_LOCATION_CURRENT(), "Launching updater...\n\tArgs: {}", args);
				Platform::Processes::Launch(path, args);

				LogWarning(MH_SOURCE_LOCATION_CURRENT(), "Exiting now for portable-mode update...");
				std::exit(1);
			});
	}

	static const BuildInfo::BuildVariant* FindNativeVariant(const std::vector<BuildInfo::BuildVariant>& variants)
	{
		const auto os = Platform::GetOS();
		const auto arch = Platform::GetArch();

		for (const auto& variant : variants)
		{
			if (variant.m_OS != os)
				continue;
			if (variant.m_Arch != arch)
				continue;

			return &variant;
		}

		return nullptr;
	}

	UpdateManager::AvailableUpdate::AvailableUpdate(const HTTPClient& client, UpdateManager& parent, BuildInfo&& buildInfo) :
		IAvailableUpdate(std::move(buildInfo)),
		m_HTTPClient(client.shared_from_this()),
		m_Parent(parent)
	{
		m_Updater = FindNativeVariant(m_BuildInfo.m_Updater);
		m_Portable = FindNativeVariant(m_BuildInfo.m_Portable);
	}

	bool UpdateManager::AvailableUpdate::CanSelfUpdate() const
	{
		if (Platform::IsInstalled())
		{
			// Installed
			return Platform::CanInstallUpdate(m_BuildInfo);
		}
		else
		{
			// Portable mode
			if (!m_Updater)
			{
				DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(), "Updater not found for build {}, os {}, platform {}.",
					m_BuildInfo.m_Version, mh::enum_fmt(Platform::GetOS()), mh::enum_fmt(Platform::GetArch()));
				return false;
			}

			if (!m_Portable)
			{
				DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(), "Portable build not found for v{}, os {}, platform {}.",
					m_BuildInfo.m_Version, mh::enum_fmt(Platform::GetOS()), mh::enum_fmt(Platform::GetArch()));
				return false;
			}

			// We should be good to go
			return true;
		}

		return false;
	}

	bool UpdateManager::AvailableUpdate::BeginSelfUpdate() const
	{
		if (!CanSelfUpdate())
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "{} called when CanSelfUpdate() returned false", __func__);
			return false;
		}

		auto client = m_Parent.m_Settings.GetHTTPClient();
		if (!client)
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "client was nullptr");
			return false;
		}

		if (Platform::IsInstalled())
		{
			if (Platform::CanInstallUpdate(m_BuildInfo))
			{
				Log(MH_SOURCE_LOCATION_CURRENT(), "Platform reports being installed and able to install update");
				m_Parent.m_State = Platform::BeginInstallUpdate(m_BuildInfo, *client);
				return true;
			}
			else
			{
				LogError(MH_SOURCE_LOCATION_CURRENT(), "Platform reports being installed but cannot install update");
				return false;
			}
		}
		else
		{
			Log(MH_SOURCE_LOCATION_CURRENT(), "Platform reports *not* being installed. Running portable update.");
			m_Parent.m_State = DownloadBuild(*client, *m_Portable, *m_Updater);
			return true;
		}
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
