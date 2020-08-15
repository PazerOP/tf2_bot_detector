"use strict";

(function() {

	if (window.tf2bd == undefined)
		window.tf2bd = {};

	let stringContains = function(string, value)
	{
		return string.indexOf(value) >= 0;
	};
	let stringContainsI = function(string, value)
	{
		return stringContains(string.toLowerCase(), value.toLowerCase());
	};

	class Triplet
	{
		constructor(cpu = "unknown", os = "unknown")
		{
			this.cpu = cpu;
			this.os = os;
		}

		is_cpu_unknown()
		{
			return this.cpu == "unknown";
		}
		is_os_unknown()
		{
			return this.os == "unknown";
		}

		toString()
		{
			return `${this.cpu}-${this.os}`;
		}
	};

	window.tf2bd.TRIPLET_X86_WINDOWS = new Triplet("x86", "windows");
	window.tf2bd.TRIPLET_X64_WINDOWS = new Triplet("x64", "windows");
	window.tf2bd.TRIPLET_X64_MAC     = new Triplet("x64", "macos");
	window.tf2bd.TRIPLET_UNKNOWN     = new Triplet();

	window.tf2bd.get_github_releases = async function(iPage = 1)
	{
		let url = new URLSearchParams();
		url.append("page", iPage);

		let result = await fetch(`https://api.github.com/repos/PazerOP/tf2_bot_detector/releases?${url.toString()}`);
		let resultParsed = await result.json();
		return resultParsed;
	};

	window.tf2bd.select_download_from_release = function(releaseJson, triplet)
	{
		for (let i = 0; i < releaseJson.assets.length; i++)
		{
			let asset = releaseJson.assets[i];
			if (!asset.name.startsWith("tf2_bot_detector_"))
				continue;

			if (!stringContainsI(asset.name, triplet.toString()))
				continue;

			return asset.browser_download_url;
		}
	};

	window.tf2bd.get_latest_release = async function(bIncludePreviews = false)
	{
		let currentPage = 1;
		let releasesJson = await tf2bd.get_github_releases(currentPage);

		while (releasesJson.length > 0)
		{
			for (let i = 0; i < releasesJson.length; i++)
			{
				let release = releasesJson[i];
				if ((bIncludePreviews && release.prerelease) ||
					(!bIncludePreviews && !release.prerelease))
				{
					return release;
				}
			}

			releasesJson = await tf2bd.get_github_releases(++currentPage);
		}
	};

	window.tf2bd.get_triplet = function()
	{
		if (window.navigator.platform == "MacIntel")
			return tf2bd.TRIPLET_X64_MAC;

		if (window.navigator.platform == "Win64")
			return tf2bd.TRIPLET_X64_WINDOWS;
		if (window.navigator.platform == "Win32")
		{
			if (window.navigator.deviceMemory > 4 ||
				stringContainsI(window.navigator.userAgent, "x86_64") ||
				stringContainsI(window.navigator.userAgent, "x86-64") ||
				stringContainsI(window.navigator.userAgent, "x64") ||
				stringContainsI(window.navigator.userAgent, "amd64") ||
				stringContainsI(window.navigator.userAgent, "WOW64") ||
				stringContainsI(window.navigator.userAgent, "x64_64"))
			{
				return tf2bd.TRIPLET_X64_WINDOWS;
			}

			return tf2bd.TRIPLET_X86_WINDOWS;
		}

		return TRIPLET_UNKNOWN;
	};

})();
