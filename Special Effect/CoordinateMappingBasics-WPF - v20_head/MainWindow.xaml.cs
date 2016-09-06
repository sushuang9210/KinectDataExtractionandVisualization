//------------------------------------------------------------------------------
// <copyright file="MainWindow.xaml.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

namespace CoordinateMappingBasicsWPF
{
    using System;
    using System.ComponentModel;
    using System.Diagnostics;
    using System.Globalization;
    using System.IO;
    using System.Windows;
    using System.Windows.Media;
    using System.Windows.Media.Imaging;
    using Microsoft.Kinect;
    using System.Collections.Generic;
    /// <summary>
    /// Interaction logic for MainWindow
    /// </summary>
    public partial class MainWindow : Window, INotifyPropertyChanged
    {
        /// <summary>
        /// Indicates opaque in an opacity mask
        /// </summary>
        private const int OpaquePixel = -1;

        /// <summary>
        /// Size of the RGB pixel in the bitmap
        /// </summary>
        private readonly int bytesPerPixel = (PixelFormats.Bgr32.BitsPerPixel + 7) / 8;

        /// <summary>
        /// Active Kinect sensor
        /// </summary>
        private KinectSensor kinectSensor = null;

        /// <summary>
        /// Coordinate mapper to map one type of point to another
        /// </summary>
        private CoordinateMapper coordinateMapper = null;

        /// <summary>
        /// Reader for depth/color/body index frames
        /// </summary>
        private MultiSourceFrameReader reader = null;

        /// <summary>
        /// Bitmap to display
        /// </summary>
        private WriteableBitmap bitmap = null;

        /// <summary>
        /// Intermediate storage for receiving depth frame data from the sensor
        /// </summary>
        private ushort[] depthFrameData = null;

        /// <summary>
        /// Intermediate storage for receiving color frame data from the sensor
        /// </summary>
        private byte[] colorFrameData = null;

        /// <summary>
        /// Intermediate storage for receiving body index frame data from the sensor
        /// </summary>
        private byte[] bodyIndexFrameData = null;

        /// <summary>
        /// Intermediate storage for frame data converted to color
        /// </summary>
        private byte[] displayPixels = null;

        /// <summary>
        /// Intermediate storage for the depth to color mapping
        /// </summary>
        private ColorSpacePoint[] colorPoints = null;
       // private uint nBuffersize;
      //  private IntPtr nBuffer;
        private ushort depth;
        /// <summary>
        /// The time of the first frame received
        /// </summary>
        private long startTime = 0;

        /// <summary>
        /// Current status text to display
        /// </summary>
        private string statusText = null;

        /// <summary>
        /// Next time to update FPS/frame time status
        /// </summary>
        private DateTime nextStatusUpdate = DateTime.MinValue;

        /// <summary>
        /// Number of frames since last FPS/frame time status
        /// </summary>
        private uint framesSinceUpdate = 0;

        /// <summary>
        /// Timer for FPS calculation
        /// </summary>
        private Stopwatch stopwatch = null;
  
        /// <summary>
        /// Reader for body frames
        /// </summary>
     //   private BodyFrameReader Body_reader = null;

        /// <summary>
        /// Array for the bodies
        /// </summary>
        private Body[] bodies = null;
        public float max_y = 1080;
        public float max_x = 1920;
        public float min_x = 0;
        public float min_y = 0;
       // public uint Buffersize;
       // public IntPtr Buffer;
        /// <summary>
        /// Initializes a new instance of the MainWindow class.
        /// </summary>
        public MainWindow()
        {
            // create a stopwatch for FPS calculation
            this.stopwatch = new Stopwatch();

            // for Alpha, one sensor is supported
            this.kinectSensor = KinectSensor.GetDefault();

            if (this.kinectSensor != null)
            {
                // get the coordinate mapper
                this.coordinateMapper = this.kinectSensor.CoordinateMapper;

                // open the sensor
                this.kinectSensor.Open();

                FrameDescription depthFrameDescription = this.kinectSensor.DepthFrameSource.FrameDescription;

                int depthWidth = depthFrameDescription.Width;
                int depthHeight = depthFrameDescription.Height;

                // allocate space to put the pixels being received and converted
                this.depthFrameData = new ushort[depthWidth * depthHeight];
                this.bodyIndexFrameData = new byte[depthWidth * depthHeight];
               
                this.colorPoints = new ColorSpacePoint[depthWidth * depthHeight];

                

                FrameDescription colorFrameDescription = this.kinectSensor.ColorFrameSource.FrameDescription;
               
                int colorWidth = colorFrameDescription.Width;
                int colorHeight = colorFrameDescription.Height;
                // create the bitmap to display
                this.bitmap = new WriteableBitmap(colorWidth, colorHeight, 96.0, 96.0, PixelFormats.Bgra32, null);
                // allocate space to put the pixels being received
                this.colorFrameData = new byte[colorWidth * colorHeight * this.bytesPerPixel];
                this.displayPixels = new byte[colorWidth * colorHeight * this.bytesPerPixel];
                this.reader = this.kinectSensor.OpenMultiSourceFrameReader(FrameSourceTypes.Depth | FrameSourceTypes.Color | FrameSourceTypes.BodyIndex|FrameSourceTypes.Body);

                this.bodies = new Body[this.kinectSensor.BodyFrameSource.BodyCount];

                // open the reader for the body frames
            //    this.Body_reader = this.kinectSensor.BodyFrameSource.OpenReader();

                // set the status text
                this.StatusText = Properties.Resources.InitializingStatusTextFormat;
            }
            else
            {
                // on failure, set the status text
                this.StatusText = Properties.Resources.NoSensorStatusText;
            }

            // use the window object as the view model in this simple example
            this.DataContext = this;

            // initialize the components (controls) of the window
            this.InitializeComponent();
        }

        /// <summary>
        /// INotifyPropertyChangedPropertyChanged event to allow window controls to bind to changeable data
        /// </summary>
        public event PropertyChangedEventHandler PropertyChanged;

        /// <summary>
        /// Gets the bitmap to display
        /// </summary>
        public ImageSource ImageSource
        {
            get
            {
                return this.bitmap;
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
        /// Execute start up tasks
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            if (this.reader != null)
            {
                this.reader.MultiSourceFrameArrived += this.Reader_MultiSourceFrameArrived;
            }
        }

        /// <summary>
        /// Execute shutdown tasks
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void MainWindow_Closing(object sender, CancelEventArgs e)
        {
            if (this.reader != null)
            {
                // MultiSourceFrameReder is IDisposable
                this.reader.Dispose();
                this.reader = null;
            }

            if (this.kinectSensor != null)
            {
                this.kinectSensor.Close();
                this.kinectSensor = null;
            }

            // Body is IDisposable
            /*
            if (this.bodies != null)
            {
                foreach (Body body in this.bodies)
                {
                    if (body != null)
                    {
                        body.Dispose();
                    }
                }
            }
             */

        }

        /// <summary>
        /// Handles the user clicking on the screenshot button
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void ScreenshotButton_Click(object sender, RoutedEventArgs e)
        {
            // create a render target that we'll render our composite control to
            RenderTargetBitmap renderBitmap = new RenderTargetBitmap((int)CompositeImage.ActualWidth, (int)CompositeImage.ActualHeight, 96.0, 96.0, PixelFormats.Pbgra32);

            DrawingVisual dv = new DrawingVisual();
            using (DrawingContext dc = dv.RenderOpen())
            {
                VisualBrush brush = new VisualBrush(CompositeImage);
                dc.DrawRectangle(brush, null, new Rect(new Point(), new Size(CompositeImage.ActualWidth, CompositeImage.ActualHeight)));
            }

            renderBitmap.Render(dv);

            // create a png bitmap encoder which knows how to save a .png file
            BitmapEncoder encoder = new PngBitmapEncoder();
            encoder.Frames.Add(BitmapFrame.Create(renderBitmap));

            string time = System.DateTime.Now.ToString("hh'-'mm'-'ss", CultureInfo.CurrentUICulture.DateTimeFormat);

            string myPhotos = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);

            string path = Path.Combine(myPhotos, "KinectScreenshot-CoordinateMapping-" + time + ".png");

            // write the new file to disk
            try
            {
                // FileStream is IDisposable
                using (FileStream fs = new FileStream(path, FileMode.Create))
                {
                    encoder.Save(fs);
                }

                this.StatusText = string.Format(Properties.Resources.SavedScreenshotStatusTextFormat, path);
                this.nextStatusUpdate = DateTime.Now + TimeSpan.FromSeconds(5);
            }
            catch (IOException)
            {
                this.StatusText = string.Format(Properties.Resources.FailedScreenshotStatusTextFormat, path);
                this.nextStatusUpdate = DateTime.Now + TimeSpan.FromSeconds(5);
            }
        }

        /// <summary>
        /// Handles the depth/color/body index frame data arriving from the sensor
        /// </summary>
        /// <param name="sender">object sending the event</param>
        /// <param name="e">event arguments</param>
        private void Reader_MultiSourceFrameArrived(object sender, MultiSourceFrameArrivedEventArgs e)
        {
            MultiSourceFrameReference frameReference = e.FrameReference;

            MultiSourceFrame multiSourceFrame = null;
            DepthFrame depthFrame = null;
            ColorFrame colorFrame = null;
            BodyIndexFrame bodyIndexFrame = null;
            BodyFrame bodyFrame = null;
            try
            {
                multiSourceFrame = frameReference.AcquireFrame();

                if (multiSourceFrame != null)
                {
                    // MultiSourceFrame is IDisposable
                    using (colorFrame = multiSourceFrame.ColorFrameReference.AcquireFrame())
                    {
                        DepthFrameReference depthFrameReference = multiSourceFrame.DepthFrameReference;
                        ColorFrameReference colorFrameReference = multiSourceFrame.ColorFrameReference;
                        BodyIndexFrameReference bodyIndexFrameReference = multiSourceFrame.BodyIndexFrameReference;
                        BodyFrameReference bodyFrameReference = multiSourceFrame.BodyFrameReference;
                        /*
                        if (this.startTime == 0)
                        {
                            this.startTime = depthFrameReference.RelativeTime;
                        }
                        */
                        depthFrame = depthFrameReference.AcquireFrame();
                      //  colorFrame = colorFrameReference.AcquireFrame();
                        bodyIndexFrame = bodyIndexFrameReference.AcquireFrame();
                        bodyFrame = bodyFrameReference.AcquireFrame();


                        //if ((depthFrame != null) && (colorFrame != null) && (bodyIndexFrame != null) && (bodyFrame != null))
                        if ((depthFrame != null) && (colorFrame != null) && (bodyIndexFrame != null) && (bodyFrame != null))
                        {
                            this.framesSinceUpdate++;
                           // uint Buffersize;
                           // IntPtr Buffer;
                            FrameDescription depthFrameDescription = depthFrame.FrameDescription;
                            FrameDescription colorFrameDescription = colorFrame.FrameDescription;
                            FrameDescription bodyIndexFrameDescription = bodyIndexFrame.FrameDescription;
                           // depthFrame.CopyFrameDataToBuffer(this.nBuffersize, this.nBuffer);

                            // update status unless last message is sticky for a while
                            if (DateTime.Now >= this.nextStatusUpdate)
                            {
                                // calcuate fps based on last frame received
                                double fps = 0.0;

                                if (this.stopwatch.IsRunning)
                                {
                                    this.stopwatch.Stop();
                                    fps = this.framesSinceUpdate / this.stopwatch.Elapsed.TotalSeconds;
                                    this.stopwatch.Reset();
                                }

                                this.nextStatusUpdate = DateTime.Now + TimeSpan.FromSeconds(1);
                               // this.StatusText = string.Format(Properties.Resources.StandardStatusTextFormat, fps, depthFrameReference.RelativeTime - this.startTime);
                            }

                            if (!this.stopwatch.IsRunning)
                            {
                                this.framesSinceUpdate = 0;
                                this.stopwatch.Start();
                            }

                            int depthWidth = depthFrameDescription.Width;
                          //  int depthHeight = depthFrameDescription.Height;
                            int depthHeight = depthFrameDescription.Height;
                           //float neck = colorFrameDescription.Height;
                           float neck_y = colorFrameDescription.Height;
                           float neck_x = colorFrameDescription.Width;
                           float head_x = 0;
                           float head_y = 0;
                            /*
                           float max_y = colorFrameDescription.Height;
                           float max_x = colorFrameDescription.Width;
                           float min_x = 0;
                           float min_y = 0;
                             */
                            int colorWidth = colorFrameDescription.Width;
                            int colorHeight = colorFrameDescription.Height;

                            int bodyIndexWidth = bodyIndexFrameDescription.Width;
                            int bodyIndexHeight = bodyIndexFrameDescription.Height;

                           
                                bodyFrame.GetAndRefreshBodyData(this.bodies);
                                // float neck = depthFrameDescription.Height;
                                foreach (Body body in this.bodies)
                                {
                                    if (body.IsTracked)
                                    {
                                        // this.DrawClippedEdges(body, dc);

                                        IReadOnlyDictionary<JointType, Joint> joints = body.Joints;

                                        // convert the joint points to depth (display) space
                                        Dictionary<JointType, Point> jointPoints = new Dictionary<JointType, Point>();
                                        int i = 0;
                                        foreach (JointType jointType in joints.Keys)
                                        {
                                            //DepthSpacePoint depthSpacePoint = this.coordinateMapper.MapCameraPointToDepthSpace(joints[jointType].Position);
                                            ColorSpacePoint colorSpacePoint = this.coordinateMapper.MapCameraPointToColorSpace(joints[jointType].Position);
                                            /*
                                            if (i == 2)
                                            { neck = depthSpacePoint.Y-4; }
                                            i = i + 1;
                                            */
                                            if (i == 2)
                                            {
                                                neck_y = colorSpacePoint.Y;
                                                neck_x = colorSpacePoint.X;
                                            }
                                            else if (i == 3)
                                            {
                                                head_x = colorSpacePoint.X;
                                                head_y = colorSpacePoint.Y;
                                            }
                                            i = i + 1;
                                        }
                                        if (head_x < neck_x)
                                        {
                                            min_x = head_x;
                                            max_x = neck_x;
                                        }
                                        else
                                        {
                                            min_x = neck_x;
                                            max_x = head_x;
                                        }
                                        if (head_y < neck_y)
                                        {
                                            min_y = head_y;
                                            max_y = neck_y;
                                        }
                                        else
                                        {
                                            min_y = neck_y;
                                            max_y = head_y;
                                        }
                                        max_x = max_x + 25;
                                        max_y = max_y-10;
                                        min_x = min_x - 25;
                                        min_y = min_y - 25;

                                    }
                                }
                            

                            // verify data and write the new registered frame data to the display bitmap
                            if (((depthWidth * depthHeight) == this.depthFrameData.Length) &&
                                ((colorWidth * colorHeight * this.bytesPerPixel) == this.colorFrameData.Length) &&
                                ((bodyIndexWidth * bodyIndexHeight) == this.bodyIndexFrameData.Length))
                            {
                                depthFrame.CopyFrameDataToArray(this.depthFrameData);
                                if (colorFrame.RawColorImageFormat == ColorImageFormat.Bgra)
                                {
                                    colorFrame.CopyRawFrameDataToArray(this.colorFrameData);
                                }
                                else
                                {
                                    colorFrame.CopyConvertedFrameDataToArray(this.colorFrameData, ColorImageFormat.Bgra);
                                }

                                bodyIndexFrame.CopyFrameDataToArray(this.bodyIndexFrameData);

                                this.coordinateMapper.MapDepthFrameToColorSpace(this.depthFrameData, this.colorPoints);

                                Array.Clear(this.displayPixels, 0, this.displayPixels.Length);
                                for (int h = 0; h < depthWidth * depthHeight; h++)
                                {
                                    if (this.bodyIndexFrameData[h] != 0xff)
                                    {
                                        this.depth = this.depthFrameData[h];
                                        if (10<this.depth && this.depth < 1000)
                                            max_y = max_y + 10;
                                            h = depthWidth * depthHeight;
                                    }
                                }

                                if (max_y - min_y < 38||max_x-min_x<38)
                                { 
                                    max_y = max_y + 30;
                                    if(head_x==max_x)
                                    max_x=max_x+15;
                                    else
                                    min_x = min_x-15;
                                }

                                // loop over each row and column of the depth
                                for (int y = (int)Math.Floor(min_y); y <max_y; ++y)
                                {
                                    for (int x = (int)Math.Floor(min_x); x < max_x; ++x)
                                    {
                                        // calculate index into depth array
                                       // int depthIndex = (y * depthWidth) + x;

                                       // byte player = this.bodyIndexFrameData[depthIndex];

                                        // if we're tracking a player for the current pixel, sets its color and alpha to full
                                     //   if (player != 0xff)
                                     //   {
                                            // retrieve the depth to color mapping for the current depth pixel
                                         //   ColorSpacePoint colorPoint = this.colorPoints[depthIndex];

                                            // make sure the depth pixel maps to a valid point in color space
                                         //   int colorX = (int)Math.Floor(colorPoint.X + 0.5);
                                         //   int colorY = (int)Math.Floor(colorPoint.Y + 0.5);
                                          //  if ((colorX >= 0) && (colorX < colorWidth) && (colorY >= 0) && (colorY < colorHeight))
                                         //   {
                                                // calculate index into color array
                                               // int colorIndex = ((colorY * colorWidth) + colorX) * this.bytesPerPixel;
                                                int colorIndex = ((y * colorWidth) + x) * this.bytesPerPixel;
                                                // set source for copy to the color pixel
                                           //     int displayIndex = depthIndex * this.bytesPerPixel;
                                                this.displayPixels[colorIndex] = this.colorFrameData[colorIndex];
                                                this.displayPixels[colorIndex + 1] = this.colorFrameData[colorIndex + 1];
                                                this.displayPixels[colorIndex + 2] = this.colorFrameData[colorIndex + 2];
                                                this.displayPixels[colorIndex + 3] = 0xff;
                                         //   }
                                    //    }
                                    }
                                }

                               // this.bitmap.WritePixels(new Int32Rect(0, 0, depthWidth, depthHeight),this.displayPixels,depthWidth * this.bytesPerPixel,0);
                                
                                this.bitmap.WritePixels(
                               new Int32Rect(0, 0, colorFrameDescription.Width, colorFrameDescription.Height),
                               //this.colorFrameData,
                               this.displayPixels,
                               colorFrameDescription.Width * this.bytesPerPixel,
                               0);
                                 
                              //  if (bodyFrame != null)
                                 //   bodyFrame.GetAndRefreshBodyData(this.bodies);
                            }
                        }
                    }
                }
            }
            catch (Exception)
            {
                // ignore if the frame is no longer available
            }
            finally
            {
                // MultiSourceFrame, DepthFrame, ColorFrame, BodyIndexFrame are IDispoable
                if (depthFrame != null)
                {
                    depthFrame.Dispose();
                    depthFrame = null;
                }

                if (colorFrame != null)
                {
                    colorFrame.Dispose();
                    colorFrame = null;
                }

                if (bodyIndexFrame != null)
                {
                    bodyIndexFrame.Dispose();
                    bodyIndexFrame = null;
                }
                /*
                if (multiSourceFrame != null)
                {
                    multiSourceFrame.Dispose();
                    multiSourceFrame = null;
                }
                */
                if (bodyFrame != null)
                {
                   bodyFrame.Dispose();
                   bodyFrame = null;
                }
            }
        }
    }
}
