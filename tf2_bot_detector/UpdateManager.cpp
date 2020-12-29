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
#include <mh/error/exception_details.hpp>
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
		struct AvailableUpdate final : IAvailableUpdate
		{
			AvailableUpdate(UpdateManager& parent, BuildInfo&& buildInfo);

			bool CanSelfUpdate() const override;
			bool BeginSelfUpdate() const override;

			std::optional<BuildInfo::BuildVariant> m_Updater;
			std::optional<BuildInfo::BuildVariant> m_Portable;

			UpdateManager& m_Parent;
		};

	public:
		UpdateManager(const Settings& settings);

		void Update() override;
		mh::status_reader<UpdateStatus> GetUpdateStatus() const override { return m_State.GetUpdateStatus(); }
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
			DownloadedUpdateTool(std::filesystem::path path, std::string arguments) :
				m_Path(std::move(path)), m_Arguments(std::move(arguments))
			{
			}

			std::filesystem::path m_Path;
			std::string m_Arguments;
		};

		struct DownloadedBuild
		{
			DownloadedBuild(BuildInfo::BuildVariant updaterVariant, std::filesystem::path extractedLocation) :
				m_UpdaterVariant(std::move(updaterVariant)), m_ExtractedLocation(std::move(extractedLocation))
			{
			}

			BuildInfo::BuildVariant m_UpdaterVariant;
			std::filesystem::path m_ExtractedLocation;
		};

		enum class UpdateToolResult
		{
			Success,
			Failure,
			ExitQueued,
		};

		// These need to be std::optional<...> because there is a bug in the MSVC STL
		// https://developercommunity.visualstudio.com/content/problem/60897/c-shared-state-futuresstate-default-constructs-the.html
		using State_t = std::variant<
			std::monostate,

			std::future<std::optional<DownloadedBuild>>,
			DownloadedBuild,

			std::future<std::optional<InstallUpdate::Result>>,
			InstallUpdate::Result,

			std::future<std::optional<DownloadedUpdateTool>>,
			DownloadedUpdateTool,

			std::future<std::optional<UpdateToolResult>>,
			UpdateToolResult
		>;
		using UpdateCheckState_t = std::variant<std::future<BuildInfo>, AvailableUpdate>;

		struct StateManager
		{
			const State_t& GetVariant() const { return m_Variant; }
			UpdateCheckState_t& GetUpdateCheckVariant() { return m_UpdateCheckVariant; }
			const UpdateCheckState_t& GetUpdateCheckVariant() const { return m_UpdateCheckVariant; }

			template<typename T, typename... TArgs>
			auto Emplace(const mh::source_location& location, UpdateStatus status, const std::string_view& msg, TArgs... args) ->
				decltype(std::declval<State_t>().emplace<T>(std::move(args)...), void())
			{
				SetUpdateStatus(location, status, std::move(msg));
				DebugLog(__FUNCSIG__);
				m_Variant.emplace<T>(std::move(args)...);
			}

			template<typename T>
			auto Set(const mh::source_location& location, UpdateStatus status, const std::string_view& msg, T value) ->
				decltype(Emplace<T>(location, status, msg, std::move(value)), void())
			{
				Emplace<T>(location, status, msg, std::move(value));
			}

			void Clear(const mh::source_location& location, UpdateStatus status, const std::string_view& msg)
			{
				Emplace<std::monostate>(location, status, msg);
			}
			void ClearUpdateCheck(const mh::source_location& location, UpdateStatus status, const std::string_view& msg)
			{
				SetUpdateStatus(location, status, msg);
				DebugLog(MH_SOURCE_LOCATION_CURRENT());
				m_UpdateCheckVariant.emplace<0>();
			}

			template<typename T>
			void SetUpdateCheck(const mh::source_location& location, UpdateStatus status, const std::string_view& msg, T value)
			{
				SetUpdateStatus(location, status, msg);
				DebugLog(__FUNCSIG__);
				m_UpdateCheckVariant.emplace<T>(std::forward<T>(value));
			}

			void SetUpdateStatus(const mh::source_location& location, UpdateStatus status,
				const std::string_view& msg);
			mh::status_reader<UpdateStatus> GetUpdateStatus() const { return m_UpdateStatus; }

			template<typename TFutureResult> bool Update(const mh::source_location& location,
				UpdateStatus success, const std::string_view& successMsg,
				UpdateStatus failure, const std::string_view& failureMsg)
			{
				return UpdateVariant<TFutureResult>(m_Variant, location,
					success, successMsg, failure, failureMsg);
			}

		private:
			template<typename TFutureResult, typename TVariant>
			bool UpdateVariant(TVariant& variant, const mh::source_location& location,
				UpdateStatus success, const std::string_view& successMsg,
				UpdateStatus failure, const std::string_view& failureMsg);

			UpdateCheckState_t m_UpdateCheckVariant;
			State_t m_Variant;
			mh::status_source<UpdateStatus> m_UpdateStatus;

		} m_State;

		bool CanReplaceUpdateCheckState() const;

		inline static const std::filesystem::path DOWNLOAD_DIR_ROOT =
			std::filesystem::temp_directory_path() / "TF2 Bot Detector" / "Portable Updates";

		void CleanupOldUpdates() const;

		static std::future<std::optional<DownloadedBuild>> DownloadBuild(const HTTPClient& client,
			BuildInfo::BuildVariant tool, BuildInfo::BuildVariant updater);
		static std::future<std::optional<DownloadedUpdateTool>> DownloadUpdateTool(const HTTPClient& client,
			BuildInfo::BuildVariant updater, std::string args);
		static std::future<std::optional<UpdateToolResult>> RunUpdateTool(std::filesystem::path path, std::string args);

		bool m_IsUpdateQueued = true;
		bool m_IsInstalled;
	};

	UpdateManager::UpdateManager(const Settings& settings) :
		m_Settings(settings),
		m_IsInstalled(Platform::IsInstalled())
	{
		CleanupOldUpdates();

		assert(m_IsUpdateQueued);
		m_State.SetUpdateStatus(MH_SOURCE_LOCATION_CURRENT(),
			UpdateStatus::CheckQueued, "Initializing update check...");
	}

	template<typename TFutureResult, typename TVariant>
	bool UpdateManager::StateManager::UpdateVariant(TVariant& variant, const mh::source_location& location,
		UpdateStatus success, const std::string_view& successMsg,
		UpdateStatus failure, const std::string_view& failureMsg)
	{
		if (auto future = std::get_if<std::future<std::optional<TFutureResult>>>(&variant))
		{
			try
			{
				if (mh::is_future_ready(*future))
				{
					auto value = future->get().value();
					SetUpdateStatus(MH_SOURCE_LOCATION_CURRENT(), success, std::string(successMsg));
					DebugLog(MH_SOURCE_LOCATION_CURRENT());
					variant.emplace<TFutureResult>(std::move(value));
				}
			}
			catch (...)
			{
				const mh::exception_details details(std::current_exception());
				LogException(MH_SOURCE_LOCATION_CURRENT(), __FUNCSIG__);
				SetUpdateStatus(MH_SOURCE_LOCATION_CURRENT(), failure,
					mh::format("{}:\n\t- {}\n\t- {}", failureMsg, details.type_name(), details.m_Message));
				variant.emplace<std::monostate>();
			}

			return true;
		}

		return false;
	}

	void UpdateManager::Update()
	{
		if (m_IsUpdateQueued && CanReplaceUpdateCheckState())
		{
			if (auto client = m_Settings.GetHTTPClient())
			{
				const auto releaseChannel = m_Settings.m_ReleaseChannel.value_or(ReleaseChannel::None);
				if (releaseChannel != ReleaseChannel::None)
				{
					auto sharedClient = client->shared_from_this();

					m_State.SetUpdateCheck(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::Checking, "Checking for updates...",
						std::async([sharedClient, releaseChannel]() -> BuildInfo
							{
								const mh::fmtstr<256> url(
									"https://tf2bd-util.pazer.us/AppInstaller/LatestVersion.json?type={:v}",
									mh::enum_fmt(releaseChannel));

								DebugLog("HTTP GET {}", url);
								auto response = sharedClient->GetString(url.view());

								auto json = nlohmann::json::parse(response);

								return json.get<BuildInfo>();
							}));
				}
				else
				{
					m_State.Emplace<std::monostate>(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateCheckDisabled,
						"Update checks disabled by the user.");
				}
			}
			else
			{
				m_State.Emplace<std::monostate>(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::InternetAccessDisabled,
					"Update check skipped because internet connectivity was disabled by the user.");
			}

			m_IsUpdateQueued = false;
		}

		if (auto future = std::get_if<std::future<BuildInfo>>(&m_State.GetUpdateCheckVariant()))
		{
			try
			{
				auto client = m_Settings.GetHTTPClient();
				if (client)
				{
					if (mh::is_future_ready(*future))
					{
						AvailableUpdate update(*this, future->get());

						if (update.m_BuildInfo.m_Version <= VERSION)
						{
							m_State.SetUpdateCheck(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpToDate,
								mh::format("Up to date (v{} {:v})", VERSION, mh::enum_fmt(
									m_Settings.m_ReleaseChannel.value_or(ReleaseChannel::Public))),
								std::move(update));
						}
						else
						{
							m_State.SetUpdateCheck(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateAvailable,
								mh::format("Update available (v{} {:v})", update.m_BuildInfo.m_Version, mh::enum_fmt(
									m_Settings.m_ReleaseChannel.value_or(ReleaseChannel::Public))),
								std::move(update));
						}
					}
				}
				else
				{
					m_State.ClearUpdateCheck(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::CheckFailed,
						"Update check failed: HTTPClient unavailable");
				}
			}
			catch (const std::exception& e)
			{
				m_State.ClearUpdateCheck(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::CheckFailed,
					mh::format("Update check failed:\n\t- {}\n\t- {}", typeid(e).name(), e.what()));
			}
		}

		if (auto downloadedBuild = std::get_if<DownloadedBuild>(&m_State.GetVariant()))
		{
			[&]
			{
				auto client = m_Settings.GetHTTPClient();
				if (!client)
				{
					m_State.Clear(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateFailed,
						"Unable to begin downloading update tool: HTTPClient unavailable");
					return;
				}

				m_State.Set(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateToolDownloading,
					"New version downloaded. Downloading update tool...",

					DownloadUpdateTool(*client, downloadedBuild->m_UpdaterVariant,
						mh::format("--update-type Portable --source-path {} --dest-path {}",
							downloadedBuild->m_ExtractedLocation, Platform::GetCurrentExeDir())));
			}();
		}
		else if (auto installUpdateResult = std::get_if<InstallUpdate::Result>(&m_State.GetVariant()))
		{
			[&]
			{
				auto needsUpdateTool = std::get_if<InstallUpdate::NeedsUpdateTool>(installUpdateResult);
				if (!needsUpdateTool)
					return;

				auto availableUpdate = GetAvailableUpdate();
				if (!availableUpdate)
				{
					m_State.Clear(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateFailed,
						"Unable to begin downloading update tool: availableUpdate was nullptr");
					return;
				}

				if (!availableUpdate->m_Updater)
				{
					m_State.Clear(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateFailed,
						"Unable to begin downloading update tool: availableUpdate->m_Updater was nullptr");
					return;
				}

				auto client = m_Settings.GetHTTPClient();
				if (!client)
				{
					m_State.Clear(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateFailed,
						"Unable to begin downloading update tool: HTTPClient unavailable");
					return;
				}

				m_State.Set(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateToolDownloading,
					"Platform app updater unavailable. Downloading update tool...",

					DownloadUpdateTool(*client, *availableUpdate->m_Updater, needsUpdateTool->m_UpdateToolArgs));
			}();
		}
		else if (auto downloadedUpdateTool = std::get_if<DownloadedUpdateTool>(&m_State.GetVariant()))
		{
			m_State.Set(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::Updating,
				"Running update tool...",

				RunUpdateTool(downloadedUpdateTool->m_Path, downloadedUpdateTool->m_Arguments));
		}
		else if (auto updateResult = std::get_if<UpdateToolResult>(&m_State.GetVariant()))
		{
			if (*updateResult == UpdateToolResult::ExitQueued)
			{
				LogWarning(MH_SOURCE_LOCATION_CURRENT(), "Exiting now for portable-mode update...");
				std::exit(1);
			}
		}

		m_State.Update<DownloadedBuild>(MH_SOURCE_LOCATION_CURRENT(),
			UpdateStatus::DownloadSuccess, "Finished downloading new version.",
			UpdateStatus::DownloadFailed, "Failed to download new version.");

		m_State.Update<InstallUpdate::Result>(MH_SOURCE_LOCATION_CURRENT(),
			UpdateStatus::UpdateSuccess, "Finished running platform update.",
			UpdateStatus::UpdateFailed, "Failed to run platform update.");

		m_State.Update<DownloadedUpdateTool>(MH_SOURCE_LOCATION_CURRENT(),
			UpdateStatus::UpdateToolDownloadSuccess, "Finished downloading update tool.",
			UpdateStatus::UpdateToolDownloadFailed, "Failed to download update tool.");

		m_State.Update<UpdateToolResult>(MH_SOURCE_LOCATION_CURRENT(),
			UpdateStatus::UpdateSuccess, "Update complete.",
			UpdateStatus::UpdateFailed, "Update failed.");
	}

	auto UpdateManager::GetAvailableUpdate() const -> const AvailableUpdate*
	{
		return std::get_if<AvailableUpdate>(&m_State.GetUpdateCheckVariant());
	}

	/// <summary>
	/// Is it safe to replace the value of m_State? (make sure we won't block the thread)
	/// </summary>
	bool UpdateManager::CanReplaceUpdateCheckState() const
	{
		const auto* future = std::get_if<std::future<BuildInfo>>(&m_State.GetUpdateCheckVariant());
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
		Log(MH_SOURCE_LOCATION_CURRENT(), "{} -> {}", url, extractDir);

		DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Downloading {}...", url);
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

	void UpdateManager::CleanupOldUpdates() const
	{
		DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Cleaning up old downloaded updates from {}...", DOWNLOAD_DIR_ROOT);

		std::error_code ec;
		auto deletedCount = std::filesystem::remove_all(DOWNLOAD_DIR_ROOT, ec);

		if (ec)
		{
			DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(), "Failed to delete old downloaded updates from {}: ({}) {}",
				DOWNLOAD_DIR_ROOT, ec.value(), ec.message());
		}
		else if (deletedCount == 0)
		{
			DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Nothing to delete from {}.", DOWNLOAD_DIR_ROOT);
		}
		else
		{
			Log(MH_SOURCE_LOCATION_CURRENT(), "Deleted {} items from {}.", deletedCount, DOWNLOAD_DIR_ROOT);
		}
	}

	auto UpdateManager::DownloadBuild(const HTTPClient& client,
		BuildInfo::BuildVariant tool, BuildInfo::BuildVariant updater) ->
		std::future<std::optional<DownloadedBuild>>
	{
		const auto downloadDir = DOWNLOAD_DIR_ROOT / mh::format("tool_{}",
			std::chrono::high_resolution_clock::now().time_since_epoch().count());

		auto clientPtr = client.shared_from_this();
		return std::async([clientPtr, tool, updater, downloadDir]() -> std::optional<DownloadedBuild>
			{
				DownloadAndExtractZip(*clientPtr, tool.m_DownloadURL, downloadDir);

				return DownloadedBuild(updater, downloadDir);
			});
	}

	auto UpdateManager::DownloadUpdateTool(const HTTPClient& client, BuildInfo::BuildVariant updater,
		std::string args) -> std::future<std::optional<DownloadedUpdateTool>>
	{
		auto clientPtr = client.shared_from_this();

		return std::async([clientPtr, updater, args]() -> std::optional<DownloadedUpdateTool>
			{
				const auto downloadDir = DOWNLOAD_DIR_ROOT / mh::format("updater_{}",
					std::chrono::high_resolution_clock::now().time_since_epoch().count());

				DownloadAndExtractZip(*clientPtr, updater.m_DownloadURL, downloadDir);

				// FIXME linux
				return DownloadedUpdateTool(downloadDir / "tf2_bot_detector_updater.exe", args);
			});
	}

	auto UpdateManager::RunUpdateTool(std::filesystem::path path, std::string args) ->
		std::future<std::optional<UpdateToolResult>>
	{
		// For now, all of our paths through the update tool require that we close ourselves
		args += mh::format(" --wait-pid {}", Platform::Processes::GetCurrentProcessID());

		return std::async([path, args]() -> std::optional<UpdateToolResult>
			{
				Log(MH_SOURCE_LOCATION_CURRENT(), "Launching updater...\n\tArgs: {}", args);

				if (Platform::IsDebuggerAttached())
				{
					LogWarning(MH_SOURCE_LOCATION_CURRENT(), "Blocking update process since debugger is attached");
				}
				else
				{
					Platform::Processes::Launch(path, args);
				}

				DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Scheduling exit...");
				return UpdateToolResult::ExitQueued;
			});
	}

	static std::optional<BuildInfo::BuildVariant> FindNativeVariant(const std::vector<BuildInfo::BuildVariant>& variants)
	{
		const auto os = Platform::GetOS();
		const auto arch = Platform::GetArch();

		for (const auto& variant : variants)
		{
			if (variant.m_OS != os)
				continue;
			if (variant.m_Arch != arch)
				continue;

			return variant;
		}

		return std::nullopt;
	}

	UpdateManager::AvailableUpdate::AvailableUpdate(UpdateManager& parent, BuildInfo&& buildInfo) :
		IAvailableUpdate(std::move(buildInfo)),
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
		DebugLog(MH_SOURCE_LOCATION_CURRENT());

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
				auto clientPtr = client->shared_from_this();
				auto buildInfo = m_BuildInfo;
				auto future = std::async([buildInfo, clientPtr]() -> std::optional<InstallUpdate::Result>
					{
						return Platform::BeginInstallUpdate(buildInfo, *clientPtr).get();
					});

				m_Parent.m_State.Set(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::Updating,
					"Installing platform update...",
					std::move(future));

				return true;
			}
			else
			{
				m_Parent.m_State.Clear(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::UpdateFailed,
					"Platform reports that TF2 Bot Detector is installed, but it is unable to install updates.");
				return false;
			}
		}
		else
		{
			auto portable = m_Portable.value();
			auto updater = m_Updater.value();

			auto downloadBuildFuture = DownloadBuild(*client, portable, updater);
			m_Parent.m_State.Set(MH_SOURCE_LOCATION_CURRENT(), UpdateStatus::Downloading,
				"Downloading new build...",
				std::move(downloadBuildFuture));

			return true;
		}
	}

	UpdateManager::BaseExceptionData::BaseExceptionData(const std::type_info& type, std::string message,
		const std::exception_ptr& exception) :
		m_Type(type), m_Message(std::move(message)), m_Exception(exception)
	{
	}

	void UpdateManager::StateManager::SetUpdateStatus(
		const mh::source_location& location, UpdateStatus status, const std::string_view& msg)
	{
		if (m_UpdateStatus.set(status, msg))
			DebugLog(location, "{}: {}", mh::enum_fmt(status), msg);
	}
}

std::unique_ptr<IUpdateManager> tf2_bot_detector::IUpdateManager::Create(const Settings& settings)
{
	return std::make_unique<UpdateManager>(settings);
}
