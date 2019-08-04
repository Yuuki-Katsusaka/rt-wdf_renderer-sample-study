/*
  ==============================================================================

    RenderThread.cpp
    Created: 2 May 2016 12:45:29pm
    Author:  Maximilian Rest

  ==============================================================================
*/

/*
===============================================================================
Change: 
  output file name for windows, disable CSV output and etc , July-2019, Shun

Caution:
 At present, upsample and then downsample, except same sampling rate,
 program will stop down.
===============================================================================
*/


#include "RenderThread.h"


RenderThread::RenderThread() : ThreadWithProgressWindow ("Processing Audio file through WDF", true, true)
{
}

void RenderThread::run( )
{

    const int blockSize = myRenderParams->blockSize;
    const int numBlocks = int(ceil( myRenderParams->transportSource->getTotalLength() / blockSize ));

    myRenderParams->totalBlocks = numBlocks;


    AudioSampleBuffer buf(1, blockSize);
    AudioSourceChannelInfo myACHI( buf );

    WavAudioFormat wav;

    File myFile ("output-rtwdf.wav");  // change output file name for windows 10
    TemporaryFile tempFile(myFile);
    ScopedPointer <OutputStream> outStream (tempFile.getFile().createOutputStream());

// disable CSV output
#ifdef  CSV_OUT   
    File myDatalogFile ("datalog.csv");
    TemporaryFile tempDatalogFile(myDatalogFile);
    ScopedPointer <OutputStream> tempDatalogFileStream(tempDatalogFile.getFile().createOutputStream());
#endif
    if (outStream != nullptr)
    {
        ScopedPointer <AudioFormatWriter> writer (wav.createWriterFor (outStream, myRenderParams->outputSampleRate, 1, myRenderParams->outputBitDepth, NULL, 0));
        if (writer != nullptr)
        {
            outStream.release();
            float outVoltage[1];

            myRenderParams->transportSource->setPosition(0);
            myRenderParams->transportSource->start();
            myRenderParams->renderTime = 0;
            myRenderParams->badBlocks = 0;
            double blockTime =  (double)myRenderParams->blockSize / myRenderParams->inputSampleRate;

            for ( int n = 0; n <= numBlocks; n++)
            {
                if (threadShouldExit())
                {
                    myRenderParams->transportSource->stop();

                    outStream = nullptr;
                    tempFile.deleteTemporaryFile();
                    writer = nullptr;

#ifdef CSV_OUT
                    tempDatalogFileStream = nullptr;
                    tempDatalogFile.deleteTemporaryFile();
#endif
                    return;
                }

                myRenderParams->transportSource->getNextAudioBlock( myACHI );

                for (int sample = 0; sample < myACHI.numSamples; sample++)
                {
                    myRenderParams->upBuf[sample] = (double)myACHI.buffer->getSample(0, sample);
                }

                double* upDataPtr;


                double currentTime =  Time::getCurrentTime().toMilliseconds();

                int numUpSamples = myRenderParams->upSmplr24->process(myRenderParams->upBuf, myACHI.numSamples, upDataPtr);


                for (int sample = 0; sample < numUpSamples; sample++)
                {
                    float inVoltage = (float) upDataPtr[sample];

                    myRenderParams->myWdfTree->setInputValue(inVoltage);
                    myRenderParams->myWdfTree->cycleWave();
                    myRenderParams->downBuf[sample] = { (float)(myRenderParams->myWdfTree->getOutputValue()) };
                }


                double* downDataPtr;

                int numDownSamples = myRenderParams->downSmplr24->process(myRenderParams->downBuf, numUpSamples, downDataPtr);

                double blockRenderTime = (Time::getCurrentTime().toMilliseconds() - currentTime)/(double)1000.0;
                myRenderParams->renderTime += blockRenderTime;
                if (blockRenderTime > blockTime)
                {
                    myRenderParams->badBlocks++;
                }



                for (int sample = 0; sample < numDownSamples; sample++)
                {
                    outVoltage[0] = { (float)(downDataPtr[sample]) };
                    float const *const tmp[] = { outVoltage, 0 };
                    writer->writeFromFloatArrays( tmp, 1, 1);
#ifdef CSV_OUT
                    tempDatalogFileStream->writeText(String(outVoltage[0]) + ",\n", false, false,"\\n");
#endif              
				}


#define DISP_COUNT 20
                if ( (n % DISP_COUNT) == 0)
                {
                    setProgress ( n / (double)numBlocks );
                 }
            }

            myRenderParams->transportSource->stop();


        }
        writer = nullptr;
        outStream = nullptr;
        tempFile.overwriteTargetFileWithTemporary();
#ifdef CSV_OUT
        tempDatalogFileStream = nullptr;
        tempDatalogFile.overwriteTargetFileWithTemporary();
#endif
    }


}

void RenderThread::setRenderParamsPtr( RenderParams* myRenderParams )
{
    this->myRenderParams = myRenderParams;
}
