/*
	Author: Wenyu
	Date: 2/25/2019
	Version: 1.2
	Env: Opencv 3.4 vc14, VS2015 Release x64
	Function:
	v1.0: process gaze data and model an ANN from 12-D inputs to 2-D screen points
	v1.1: add mat release, split cpp file, add destructor
	v1.2: add static shuffle function for data preprocessing, add time consumption
		analysis, change the model
*/

#include "gazeEstimate.h"

#include <ctime>
#include <cmath>

GazeEst::GazeEst() {
	m_train_time = -1;
	m_pre_time = -1;
}

GazeEst::~GazeEst() {
	m_network.release();
}

void GazeEst::create() {
	int N = 5;
	Mat layerSizes = (Mat_<int>(1, N) << 12, 50, 25, 12, 2);
	m_network = ANN_MLP::create();
	m_network->setLayerSizes(layerSizes);
	m_network->setActivationFunction(ANN_MLP::SIGMOID_SYM, 1, 1);
	m_network->setTermCriteria(
		TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, 
			50000, 1e-8f/*FLT_EPSILON*/));
	m_network->setTrainMethod(ANN_MLP::BACKPROP, 0.001, 0.001);
}

float GazeEst::train(const Mat& trainInputs, const Mat& trainOutputs) {
	// TODO: check dims
	// scale inputs and labels
	Mat trainData = Mat_<float>(trainInputs.rows, trainInputs.cols);
	Mat trainLabel = Mat_<float>(trainOutputs.rows, trainOutputs.cols);
	for (int i = 0; i < trainInputs.rows; ++i) {
		for (int j = 0; j < trainInputs.cols; ++j) {
			trainData.at<float>(i, j) = trainInputs.at<float>(i, j) / x_scale[j];
		}
		for (int k = 0; k < trainOutputs.cols; ++k) {
			trainLabel.at<float>(i, k) = trainOutputs.at<float>(i, k) / y_scale[k];
		}
	}

	// train
	Ptr<TrainData> tD = TrainData::create(
		trainData,
		ROW_SAMPLE,
		trainLabel);

	time_t t_start = clock();
	m_network->train(tD);
	m_train_time = double(clock() - t_start) / CLOCKS_PER_SEC;

	// test
	Mat trainPredicts = Mat_<float>(trainInputs.rows, 2);
	predict(trainInputs, trainPredicts);
	Mat trainError;
	absdiff(trainOutputs, trainPredicts, trainError);
	Scalar s = mean(trainError);

	trainPredicts.release();
	trainError.release();

	trainData.release();
	trainLabel.release();
	return float(s[0]);
}

double GazeEst::getTrainTime() {
	return m_train_time;
}

void GazeEst::load(const char* fileName) {
	m_network = ANN_MLP::load(fileName);
}

void GazeEst::save(const char* fileName) {
	m_network->save(fileName);
}

float GazeEst::predict(const Mat& testInputs, Mat& testOutputs, const Mat& testLabels) {
	// TODO: check dims
	// scale inputs
	Mat testData = Mat_<float>(testInputs.rows, testInputs.cols);
	for (int i = 0; i < testInputs.rows; ++i) {
		for (int j = 0; j < testInputs.cols; ++j) {
			testData.at<float>(i, j) = testInputs.at<float>(i, j) / x_scale[j];
		}
	}
	// predict
	Mat predictLabel;

	time_t t_start = clock();
	m_network->predict(testData, predictLabel);
	m_pre_time = double(clock() - t_start) / CLOCKS_PER_SEC / testData.rows;

	// rescale outputs
	for (int i = 0; i < testInputs.rows; ++i) {
		for (int j = 0; j < 2; ++j) {
			testOutputs.at<float>(i, j) = predictLabel.at<float>(i, j) * y_scale[j];
		}
	}

	testData.release();
	predictLabel.release();

	// test
	if (testLabels.rows == testOutputs.rows
		&& testLabels.cols == testOutputs.cols)
	{
		Mat testError;
		absdiff(testOutputs, testLabels, testError);
		Scalar s = mean(testError);
		testError.release();
		return float(s[0]);
	}
	else {
		return -1;
	}
}

double GazeEst::getTestTime() {
	return m_pre_time;
}

void GazeEst::shuffle(const Mat& src, Mat& dst) {
	// index not exceed 65535
	int n = src.rows;
	Mat l_dst= Mat_<float>(src.rows, src.cols);

	srand(time(NULL));
	int* sIdx = new int[n];

	// init sequence
	for (int i = 0; i < n; ++i) {
		sIdx[i] = i;
	}

	// shuffle sequence index
	for (int j = 0; j < n; j++) {
		int r = rand() % (n - j);
		int tp = sIdx[r];
		sIdx[r] = sIdx[n - j - 1];
		sIdx[n - j - 1] = tp;
	}

	// reorder the data
	for (int k = 0; k < n; ++k) {
		for (int l = 0; l < src.cols; ++l) {
			l_dst.at<float>(k, l) = src.at<float>(sIdx[k], l);
		}
	}
	dst = l_dst.clone();
	l_dst.release();
	delete[] sIdx;
}