#include <iostream>
#include <Eigen/Geometry>
#include "utils/FrameStream.h"
#include "tsdfvh/tsdf_volume.h"
#include "tracker/tracker.h"
#include <chrono>
#include <iomanip>
#include <settings.h>
#include <Logger.h>
#include <cpptoml.h>

using namespace std;
using namespace refusion;

int main(int argc, char** argv)
{
	// For now, config file will be in a fixed dir. Here we read in all the settings.
	shared_ptr<cpptoml::table> config = (cpptoml::parse_file("/app/config.toml"));

	// Use the read in settings to create the settings structs.
	settings settings = getSettings(*config);
	tsdfvh::TsdfVolumeOptions tsdf_options = getTsdfVolumeOptions(*config);
	TrackerOptions tracker_options = getTrackerOptions(*config);
	RgbdSensor sensor = getSensorConfig(*config);

	// Create the logger
	Logger logger {settings.verbose, settings.debug, settings.filepath};

	logger.alwaysLog("Settings loaded. Starting reconstruction...");
	logger.verboseLog("Logger created. Verbosity is turned on.");

	// Protect against no video being provided.
	if (argc == 0) {
		logger.error("No video provided. Ending.");
		exit(EXIT_FAILURE);
	}

	logger.verboseLog("File provided: " + (string)argv[1] + ".");
	logger.verboseLog("Reading frames...");

	// Create TUMVideo object
	TUMVideo video {argv[1], settings.streamFrames};

	logger.verboseLog("Frames read.");
	logger.verboseLog("Creating tracker...");

	// Create tracker
	refusion::Tracker tracker {tsdf_options, tracker_options, sensor};

	logger.verboseLog("Tracker created.");

	// Create output file
	std::ofstream result((string)argv[1] + ".txt");

	logger.verboseLog("Output file created.");
	logger.verboseLog("Starting main loop...");

	// Start main loop
	while (!video.finished())
	{
		logger.debugLog("Processing frame " + to_string(video.getCurrentFrameIndex()) + ".");

		Frame frame = video.nextFrame();
		tracker.AddScan(frame.rgb, frame.depth);

		// Get the current pose
		Eigen::Matrix4d pose = tracker.GetCurrentPose();
		Eigen::Quaterniond rotation(pose.block<3, 3>(0, 0));

		// Write the pose to file
		result << std::fixed << std::setprecision(6) << video.getCurrentTimestamp()
			<< " " << pose.block<3, 1>(0, 3).transpose() << " "
			<< rotation.vec().transpose() << " " << rotation.w() << std::endl;
	}

	logger.verboseLog("Main loop finished.");

	// Create mesh
	logger.verboseLog("Saving mesh...");
	float3 low_limits = make_float3(-3, -3, 0);
	float3 high_limits = make_float3(3, 3, 4);
	refusion::tsdfvh::Mesh *mesh;
	cudaMallocManaged(&mesh, sizeof(refusion::tsdfvh::Mesh));
	*mesh = tracker.ExtractMesh(low_limits, high_limits);
	mesh->SaveToFile((string)argv[1] + ".txt");

	logger.verboseLog("Mesh saved.");
	logger.alwaysLog("Done.");

	return EXIT_SUCCESS;
}