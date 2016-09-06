//------------------------------------------------------------------------------
// <copyright file="MainWindow.xaml.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------
namespace Microsoft.Samples.Kinect.ColorBasics
{
    using System;
    using System.ComponentModel;
    using System.Diagnostics;
    using System.Globalization;
    using System.IO;
    using System.Windows;
    using System.Windows.Media;
    using System.Windows.Media.Imaging;
    using System.Threading;
    using Microsoft.Kinect;
    using Microsoft.Kinect.Tools;
    /// <summary>
    /// Interaction logic for MainWindow
    /// </summary>
    public partial class MainWindow : Window, INotifyPropertyChanged
    {
        /// <summary>
        /// Active Kinect sensor
        /// </summary>
        private KinectSensor kinectSensor = null;

        /// <summary>
        /// Reader for color frames
        /// </summary>
        private ColorFrameReader colorFrameReader = null;

        /// <summary>
        /// Bitmap to display
        /// </summary>
        private WriteableBitmap colorBitmap = null;

        /// <summary>
        /// Current status text to display
        /// </summary>
        private string statusText = null;
       // KStudioRecording
       // KStudioClient kStudioClient = KStudio.CreateClient();

        /// <summary>
        /// Initializes a new instance of the MainWindow class.
        /// </summary>
        public int Day=0;
        public int Hour = 0;
        public int Minute = 0;
        public int Second = 0;
        public int open = 1;
       // public StreamWriter sw = new StreamWriter("1.txt", true);

        public MainWindow()
        {
            if (this.open == 1)
            {
                // get the kinectSensor object
                this.kinectSensor = KinectSensor.GetDefault();

                // open the reader for the color frames
                this.colorFrameReader = this.kinectSensor.ColorFrameSource.OpenReader();

                // wire handler for frame arrival
                this.colorFrameReader.FrameArrived += this.Reader_ColorFrameArrived;

                // create the colorFrameDescription from the ColorFrameSource using Bgra format
                FrameDescription colorFrameDescription = this.kinectSensor.ColorFrameSource.CreateFrameDescription(ColorImageFormat.Bgra);

                // create the bitmap to display
                this.colorBitmap = new WriteableBitmap(colorFrameDescription.Width, colorFrameDescription.Height, 96.0, 96.0, PixelFormats.Bgr32, null);

                // set IsAvailableChanged event notifier
                this.kinectSensor.IsAvailableChanged += this.Sensor_IsAvailableChanged;
                /*
                string time = System.DateTime.Now.ToString("hh'.'mm'.'ss", CultureInfo.CurrentUICulture.DateTimeFormat);
                //string time = System.DateTime.Now.
                string myPhotos = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);

                string path = Path.Combine(myPhotos, "KinectScreenshot-Color-" + time);
                TimeSpan timespan = new TimeSpan(0, 0, 1, 0);
                 RecordClip(path, timespan);
                 */
                // open the sensor
                //if(this.open==1)
                this.kinectSensor.Open();

                // set the status text
                this.StatusText = this.kinectSensor.IsAvailable ? Properties.Resources.RunningStatusText
                                                                : Properties.Resources.NoSensorStatusText;

                // use the window object as the view model in this simple example
                this.DataContext = this;

                StreamReader sr = new StreamReader("C://name.txt");
                string content = sr.ReadToEnd();
                string[] path = content.Split(new string[] { "\r\n" }, StringSplitOptions.None);
                int row;
                StreamReader str1 = new StreamReader(new FileStream("C://row.txt", FileMode.Open, FileAccess.Read));
                string content1 = str1.ReadToEnd().Replace("\r\n", " ");
                string[] split = content1.Split(new string[] { "\r\n" }, StringSplitOptions.None);
                row = int.Parse(split[0]);
                //path = "C://2014-09-03-16-58-34-966.xef";
                //PlaybackClip(path[row], 0);
                using (KStudioClient client = KStudio.CreateClient())
                {
                    client.ConnectToService();

                    using (KStudioPlayback playback = client.CreatePlayback(path[row]))
                    {
                        playback.LoopCount = 0;
                        playback.Start();
                        if (playback.State != KStudioPlaybackState.Playing)
                        {
                            //this.MainWindow_Closing(this.kinectSensor,CancelEventArgs e);
                            if (this.colorFrameReader != null)
                            {
                                // ColorFrameReder is IDisposable
                                this.colorFrameReader.Dispose();
                                this.colorFrameReader = null;
                            }

                            if (this.kinectSensor != null)
                            {
                                this.kinectSensor.Close();
                                this.kinectSensor = null;
                            }
                            // sw.Close();
                            this.open = 0;
                        }
                        while (playback.State == KStudioPlaybackState.Playing)
                        {
                            Thread.Sleep(500);
                        }

                        if (playback.State == KStudioPlaybackState.Error)
                        {
                            throw new InvalidOperationException("Error: Playback failed!");
                        }
                        playback.Stop();
                       // File.WriteAllText(@"C:\end.txt", "1");
                    }

                    client.DisconnectFromService();
                }

                // initialize the components (controls) of the window
                this.InitializeComponent();
            }
        }

        /// <summary>
        /// INotifyPropertyChangedPropertyChanged event to allow window controls to bind to changeable data
        /// </summary>.
        public event PropertyChangedEventHandler PropertyChanged;

        /// <summary>
        /// Gets the bitmap to display
        /// </summary>
        public ImageSource ImageSource
        {
            get
            {
                return this.colorBitmap;
            }
        }

        /// <summary>
        /// Gets or sets the current status text to display
        /// </summary>
        public string StatusText
        {
            get
            {
                return this.statusText;
            }

            set
            {
                if (this.statusText != value)
                {
                    this.statusText = value;

                    // notify any bound elements that the text has changed
                    if (this.PropertyChanged != null)
                    {
                        this.PropertyChanged(this, new PropertyChangedEventArgs("StatusText"));
                    }
                }
            }
        }

        /// <summary>
        /// Execute shutdown tasks
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void MainWindow_Closing(object sender, CancelEventArgs e)
        {
            if (this.colorFrameReader != null)
            {
                // ColorFrameReder is IDisposable
                this.colorFrameReader.Dispose();
                this.colorFrameReader = null;
            }

            if (this.kinectSensor != null)
            {
                this.kinectSensor.Close();
                this.kinectSensor = null;
            }
            //sw.Close();
        }

        /// <summary>
        /// Handles the user clicking on the screenshot button
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void ScreenshotButton_Click(object sender, RoutedEventArgs e)
        {
            if (this.colorBitmap != null)
            {
                // create a png bitmap encoder which knows how to save a .png file
                BitmapEncoder encoder = new PngBitmapEncoder();

                // create frame from the writable bitmap and add to encoder
                encoder.Frames.Add(BitmapFrame.Create(this.colorBitmap));

                string time = System.DateTime.Now.ToString("hh'-'mm'-'ss", CultureInfo.CurrentUICulture.DateTimeFormat);
                
                string myPhotos = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);

                string path = Path.Combine(myPhotos, "KinectScreenshot-Color-" + time + ".png");

                // write the new file to disk
                try
                {
                    // FileStream is IDisposable
                    using (FileStream fs = new FileStream(path, FileMode.Create))
                    {
                        encoder.Save(fs);
                    }

                    this.StatusText = string.Format(Properties.Resources.SavedScreenshotStatusTextFormat, path);
                }
                catch (IOException)
                {
                    this.StatusText = string.Format(Properties.Resources.FailedScreenshotStatusTextFormat, path);
                }
            }
        }

        /// <summary>
        /// Handles the color frame data arriving from the sensor
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void Reader_ColorFrameArrived(object sender, ColorFrameArrivedEventArgs e)
        {
            //this.Hour = System.DateTime.Now.Hour;
            //this.Minute = System.DateTime.Now.Minute;
           // this.Second = System.DateTime.Now.Second;
            string time = System.DateTime.Now.ToString("hh'.'mm'.'ss", CultureInfo.CurrentUICulture.DateTimeFormat);
           // this.fs.Write(time, 0, time.Length);
            //sw.Write(time);
            //sw.WriteLine(time);
            //sw.Flush();
            string myPhotos = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);
           // string path = Path.Combine(myPhotos, "KinectScreenshot-Color-7" + ".xef");

            /*
            StreamReader sr = new StreamReader("C://name.txt");
            string content=sr.ReadToEnd();
            string[] path = content.Split(new string[] { "\r\n" }, StringSplitOptions.None);
            int row;
            StreamReader str1 = new StreamReader(new FileStream("C://row.txt",FileMode.Open,FileAccess.Read));
            string content1 = str1.ReadToEnd().Replace("\r\n"," ");
            string[] split = content1.Split(new string[] { "\r\n" }, StringSplitOptions.None);
            row = int.Parse(split[0]);
           //path = "C://2014-09-03-16-58-34-966.xef";
            PlaybackClip(path[row], 0);
             */
             
            // ColorFrame is IDisposable
            using (ColorFrame colorFrame = e.FrameReference.AcquireFrame())
            {
                if (colorFrame != null)
                {
                    FrameDescription colorFrameDescription = colorFrame.FrameDescription;

                    using (KinectBuffer colorBuffer = colorFrame.LockRawImageBuffer())
                    {
                        this.colorBitmap.Lock();

                        // verify data and write the new color frame data to the display bitmap
                        if ((colorFrameDescription.Width == this.colorBitmap.PixelWidth) && (colorFrameDescription.Height == this.colorBitmap.PixelHeight))
                        {
                            colorFrame.CopyConvertedFrameDataToIntPtr(
                                this.colorBitmap.BackBuffer,
                                (uint)(colorFrameDescription.Width * colorFrameDescription.Height * 4),
                                ColorImageFormat.Bgra);

                            this.colorBitmap.AddDirtyRect(new Int32Rect(0, 0, this.colorBitmap.PixelWidth, this.colorBitmap.PixelHeight));
                        }

                        this.colorBitmap.Unlock();
                    }
                }
            }
            
           // string time = System.DateTime.Now.ToString("hh'.'mm'.'ss", CultureInfo.CurrentUICulture.DateTimeFormat);
            //string time = System.DateTime.Now.
            //string myPhotos = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);

           // string path = Path.Combine(myPhotos, "KinectScreenshot-Color-7" + ".xef");
           // TimeSpan timespan = new TimeSpan(0, 0, 10, 0);
           // RecordClip(path,timespan);
             
        }

        /// <summary>
        /// Handles the event which the sensor becomes unavailable (E.g. paused, closed, unplugged).
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void Sensor_IsAvailableChanged(object sender, IsAvailableChangedEventArgs e)
        {
            // on failure, set the status text
            this.StatusText = this.kinectSensor.IsAvailable ? Properties.Resources.RunningStatusText
                                                            : Properties.Resources.SensorNotAvailableStatusText;
        }

public static void RecordClip(string filePath, TimeSpan duration)
 //public static void RecordClip(string filePath)
{
    using (KStudioClient client = KStudio.CreateClient())
    {
     client.ConnectToService();

KStudioEventStreamSelectorCollection streamCollection = new KStudioEventStreamSelectorCollection();
           // streamCollection.Add(KStudioEventStreamDataTypeIds.Ir);
            streamCollection.Add(KStudioEventStreamDataTypeIds.Depth);
            streamCollection.Add(KStudioEventStreamDataTypeIds.Body);
            streamCollection.Add(KStudioEventStreamDataTypeIds.BodyIndex);
           streamCollection.Add(KStudioEventStreamDataTypeIds.UncompressedColor);
            //streamCollection.Add(KStudioEventStreamDataTypeIds.CompressedColor);
           // using(KStudioRecording recording = client.CreateRecording(filePath, streamCollection))
           // {}
        
       using (KStudioRecording recording = client.CreateRecording(filePath, streamCollection))
       {
           recording.StartTimed(duration);
           
           while (recording.State == KStudioRecordingState.Recording)
           {
               Thread.Sleep(500);
              
           }
           /*
           if (recording.State == KStudioRecordingState.Error)
           {
               throw new InvalidOperationException("Error: Recording failed!");
           }
            */
       }
       
//client.DisconnectFromService();
    }
}

/*
public static void PlaybackClip(string filePath, uint loopCount)
{
    using (KStudioClient client = KStudio.CreateClient())
    {
       client.ConnectToService();
       
       using (KStudioPlayback playback = client.CreatePlayback(filePath))
       {
           playback.LoopCount = loopCount;
           playback.Start();


           while (playback.State == KStudioPlaybackState.Playing)
           {
                Thread.Sleep(500);
           }

           if (playback.State == KStudioPlaybackState.Error)
           {
               throw new InvalidOperationException("Error: Playback failed!");
           }
           playback.Stop();
       }

       client.DisconnectFromService();
    }
}
*/

private void MainWindow_opening(object sender, EventArgs e)
{
    this.Day = System.DateTime.Now.Day;
    this.Hour = System.DateTime.Now.Hour;
    this.Minute = System.DateTime.Now.Minute;
    this.Second = System.DateTime.Now.Second;

}

    }
}
