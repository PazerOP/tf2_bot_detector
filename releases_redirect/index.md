<script src="../js/redirect.js"></script>
<script>
	(async function() {
		let queryParams = new URLSearchParams(window.location.search);

		let triplet = tf2bd.get_triplet();
		console.log(`Triplet: ${triplet}`);

		if (queryParams.has("cpu"))
			triplet.cpu = queryParams.get("cpu");
		if (queryParams.has("os"))
			triplet.os = queryParams.get("os");

		let latestRelease = await tf2bd.get_latest_release(queryParams.has("preview"));
		console.log(`Latest release: ${latestRelease.name}`);

		let url = tf2bd.select_download_from_release(latestRelease, triplet);
		console.log(`Download URL: ${url}`);

		window.location.href = url;
	})();
</script>

# Downloading the latest release...
If your browser gets stuck here, try manually selecting the latest release by [clicking here](https://github.com/PazerOP/tf2_bot_detector/releases/latest).
