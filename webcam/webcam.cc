/**
 * This file is part of ORB-SLAM3
 *
 * Copyright (C) 2017-2021 Carlos Campos, Richard Elvira, Juan J. Gómez Rodríguez, José M.M. Montiel and Juan D. Tardós, University of Zaragoza.
 * Copyright (C) 2014-2016 Raúl Mur-Artal, José M.M. Montiel and Juan D. Tardós, University of Zaragoza.
 *
 * ORB-SLAM3 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ORB-SLAM3 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with ORB-SLAM3.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <signal.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>

#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

#include <System.h>

using namespace std;

bool b_continue_session;

void exit_loop_handler(int s)
{
    cout << "Finishing session" << endl;
    b_continue_session = false;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 4)
    {
        cerr << endl
             << "Usage: ./mono_webcam path_to_vocabulary path_to_settings (trajectory_file_name)" << endl;
        return 1;
    }

    string file_name;
    bool bFileName = false;

    if (argc == 4)
    {
        file_name = string(argv[argc - 1]);
        bFileName = true;
    }

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = exit_loop_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    b_continue_session = true;

    // Initialize USB webcam
    cv::VideoCapture cap(0); // Use device 0 (default camera)
    if (!cap.isOpened())
    {
        cerr << "Error: Unable to open webcam" << endl;
        return -1;
    }

    // Set camera resolution (optional)
    int width_img = 640;  // Set this based on your webcam
    int height_img = 480; // Set this based on your webcam
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width_img);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height_img);
    cap.set(cv::CAP_PROP_EXPOSURE, -6); // Decrease exposure for faster shutter speed
    cap.set(cv::CAP_PROP_FPS, 30);      // Set frame rate to 60 FPS

    // Create SLAM system. It initializes all system threads and gets ready to process frames.
    ORB_SLAM3::System SLAM(argv[1], argv[2], ORB_SLAM3::System::MONOCULAR, true);
    float imageScale = SLAM.GetImageScale();

    cv::Mat imCV;
    double t_resize = 0.f;
    double t_track = 0.f;

    while (b_continue_session)
    {
        double timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();

        cap >> imCV; // Capture a frame
        if (imCV.empty())
        {
            cerr << "Error: Webcam frame is empty" << endl;
            break;
        }

        // Resize if necessary
        if (imageScale != 1.f)
        {
            int width = imCV.cols * imageScale;
            int height = imCV.rows * imageScale;
            cv::resize(imCV, imCV, cv::Size(width, height));
        }

#ifdef REGISTER_TIMES
        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
#endif

        // Pass the image to the SLAM system
        SLAM.TrackMonocular(imCV, timestamp_ms);

#ifdef REGISTER_TIMES
        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
        t_track = t_resize + std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t2 - t1).count();
        SLAM.InsertTrackTime(t_track);
#endif
    }

    // Stop all threads
    SLAM.Shutdown();

    return 0;
}
