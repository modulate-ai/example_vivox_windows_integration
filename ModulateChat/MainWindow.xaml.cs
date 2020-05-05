using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

using ModulateVivoxWrapper;

using System.IO;
using System.Net.Http;
using System.Threading;
using System.Security.Cryptography;

namespace ModulateChat
{
    public partial class MainWindow : Window
    {
        private ModulateVivoxManagedWrapper modulate;
        private string[] voice_skin_names;
        private string[] voice_skin_display_names;
        private bool echo_running = false;
        private bool connected = false;
        private string channel_name;

        private bool has_thrown_fatal_error = false;

        private string modulate_log_folder;
        private Timer log_size_timer;
        private string channel_name_prefix;

        private static readonly HttpClient client = new HttpClient();
        const int VIVOX_TIMEOUT = 10000;

        public MainWindow()
        {
            InitializeComponent();

            string app_data_folder = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            modulate_log_folder = System.IO.Path.Combine(app_data_folder, "ModulateChat", "modulate_logs");
            System.IO.Directory.CreateDirectory(modulate_log_folder);

            string app_directory = System.AppDomain.CurrentDomain.BaseDirectory;
            string voice_skin_folder = System.IO.Path.Combine(app_directory, "voice_skins");
            if (!Directory.Exists(voice_skin_folder))
                fatal_error("Couldn't find voice skin folder at "+voice_skin_folder);
            string api_key_file = System.IO.Path.Combine(voice_skin_folder, "api_key.txt");
            if (!File.Exists(api_key_file))
                fatal_error("Couldn't find API key file at " + api_key_file);
            string[] voice_skin_files = Directory.GetFiles(voice_skin_folder, "*.mod", SearchOption.TopDirectoryOnly);
            if (voice_skin_files.Length == 0)
                fatal_error("Found 0 voice skin files with extension .mod in " + voice_skin_folder);

            modulate = new ModulateVivoxManagedWrapper(modulate_log_folder);
            Console.WriteLine("Using Modulate version {0}", modulate.version());

            System.IO.StreamReader api_key_file_reader = new System.IO.StreamReader(api_key_file);
            string api_key = api_key_file_reader.ReadLine();
            api_key_file_reader.Close();
            if (api_key.Length == 0)
                fatal_error("Read blank API key from " + api_key_file);

            SHA256 sha256 = SHA256.Create();
            byte[] bytes = sha256.ComputeHash(Encoding.UTF8.GetBytes(api_key));
            StringBuilder tmp_hash = new StringBuilder();
            for (int i = 0; i < bytes.Length; i++)
                tmp_hash.Append(bytes[i].ToString("x2"));
            String hash = tmp_hash.ToString();

            channel_name_prefix = hash.Substring(0, 5);
            Console.WriteLine("Set channel name prefix as " + channel_name_prefix);

            log_size_timer = new Timer(update_log_size_text, new AutoResetEvent(false), 0, 10000);

            int error_code;
            error_code = modulate.load_api_key_from_file(api_key_file);
            if (error_code != 0)
                fatal_error("Couldn't load api key from " + api_key_file);
            foreach(string filename in voice_skin_files)
            {
                error_code = modulate.create_voice_skin(filename);
                if (error_code != 0)
                    fatal_error("Failed to create voice skin from file " + filename + " error code " + error_code.ToString());
            }
            List<string> voice_skin_list = new List<string>();
            for(int i = 0; i < modulate.get_number_of_skins(); i++)
            {
                voice_skin_list.Add(modulate.get_voice_skin_name(i));
            }
            voice_skin_names = voice_skin_list.ToArray();
            voice_skin_display_names = voice_skin_list.ToArray();
            string[] separators = { "_2020_" };
            for(int i = 0; i < voice_skin_display_names.Length; i++)
                voice_skin_display_names[i] = voice_skin_display_names[i].Split(separators, StringSplitOptions.None)[0];
            VoiceSkinSelector.ItemsSource = voice_skin_display_names;
            VoiceSkinSelector.SelectedIndex = 0;
            for (int i = 0; i < voice_skin_names.Length; i++)
            {
                if (voice_skin_names[i].Contains("sarena"))
                    VoiceSkinSelector.SelectedIndex = i;
            }
            modulate.select_voice_skin(voice_skin_names[VoiceSkinSelector.SelectedIndex]);

            modulate.vivox_start_connect();

            foreach(string voice_skin_name in voice_skin_names)
            {
                authenticate_skin(voice_skin_name);
            }

            bool vivox_connected = false;
            int sleep_duration = 100;
            for(int current_ms = 0; current_ms < VIVOX_TIMEOUT; current_ms += sleep_duration)
            {
                vivox_connected = modulate.vivox_check_connected() > 0;
                if (vivox_connected)
                    break;
                Thread.Sleep(sleep_duration);
            }
            Console.WriteLine("Vivox connected {0}", vivox_connected);
            if (!vivox_connected)
                fatal_error("Failed to connect to the Vivox chat server after " + (VIVOX_TIMEOUT / 1000) + " seconds.  Please check that you're connected to the internet, and if the problem persists, contact <>");
            modulate.vivox_login();
        }

        private void fatal_error(string message)
        {
            if (has_thrown_fatal_error)
                return;
            has_thrown_fatal_error = true;
            MessageBox.Show(message, "Fatal Error", MessageBoxButton.OK, MessageBoxImage.Error);
            throw new SystemException("Modulate Fatal Error: "+message);
        }

        private async void authenticate_skin(string voice_skin_name)
        {
            string auth_message = modulate.create_auth_message_for_voice_skin(voice_skin_name);
            Console.WriteLine("Authenticating " + voice_skin_name + " with auth message " + auth_message);
            HttpRequestMessage msg = new HttpRequestMessage(HttpMethod.Post, "<>");
            msg.Content = new StringContent("{\"request_string\": \"" + auth_message + "\"}", Encoding.UTF8, "application/json");
            msg.Headers.Add("Accept", "application/json");
            HttpResponseMessage response = await client.SendAsync(msg);
            string response_string = await response.Content.ReadAsStringAsync();
            Console.WriteLine("Auth response message " + response_string);

            if (!response_string.Contains("\"success\": true"))
                fatal_error("Failed to authenticate voice skin " + voice_skin_name + " with the Modulate server.  Please ensure that you're connected to the internet, and if the problem persists, contact <>");
            string response_key = "\"signed_response\": \"";
            string signed_response = response_string.Substring(response_string.IndexOf(response_key) + response_key.Length);
            signed_response = signed_response.Substring(0, signed_response.IndexOf("\""));
            Console.WriteLine("Checking "+voice_skin_name+" auth with response "+signed_response);

            int error_code = modulate.check_auth_message_for_voice_skin(voice_skin_name, signed_response);
            if (error_code != 0)
                fatal_error("Failed to authenticate voice skin " + voice_skin_name + " Please ensure that you have the latest ModulateChat app, and contact <> if the problem persists.");
        }

        private void VoiceSkinSelector_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            modulate.select_voice_skin(voice_skin_names[VoiceSkinSelector.SelectedIndex]);
        }

        private void start_echo()
        {
            modulate.vivox_start_realtime_echo();
            TestVoiceButton.Content = "Stop Echo";
            echo_running = true;
        }

        private void stop_echo()
        {
            modulate.vivox_end_realtime_echo();
            TestVoiceButton.Content = "Start Echo";
            echo_running = false;
        }

        private void TestVoiceButton_Click(object sender, RoutedEventArgs e)
        {
            if (echo_running)
                stop_echo();
            else
                start_echo();
        }

        private string fix_channel_name(string input)
        {
            StringBuilder output = new StringBuilder(input);
            string allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-";
            for (int i = 0; i < output.Length; i++)
                if (!allowed_chars.Contains(output[i]))
                    output[i] = '_';
            return output.ToString();
        }

        private void connect()
        {
            bool vivox_logged_in = false;
            int sleep_duration = 100;
            for (int current_ms = 0; current_ms < VIVOX_TIMEOUT; current_ms += sleep_duration)
            {
                vivox_logged_in = modulate.vivox_check_logged_in() > 0;
                if (vivox_logged_in)
                    break;
                Thread.Sleep(sleep_duration);
            }
            Console.WriteLine("Vivox logged in {0}", vivox_logged_in);
            if (!vivox_logged_in)
                fatal_error("Failed to login to the Vivox chat server after " + (VIVOX_TIMEOUT / 1000) + " seconds.  Please check that you're connected to the internet, and if the problem persists, contact <>");

            channel_name = channel_name_prefix + fix_channel_name(ChannelTextBox.Text);
            modulate.vivox_add_session(channel_name);
            ConnectButton.Content = "Disconnect";
            Console.WriteLine("Connected to channel " + channel_name);
            connected = true;
            TestVoiceButton.IsEnabled = true;
        }

        private void disconnect()
        {
            TestVoiceButton.IsEnabled = false;
            stop_echo();
            modulate.vivox_remove_session(channel_name);
            ConnectButton.Content = "Connect";
            Console.WriteLine("Disconnected from channel " + channel_name);
            connected = false;
        }

        private void ConnectButton_Click(object sender, RoutedEventArgs e)
        {
            if (connected)
                disconnect();
            else
                connect();
        }

        private void RadioSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            float new_value = (float)e.NewValue;
            Console.WriteLine("Setting new radio value {0}", new_value);
            modulate.set_radio_strength(new_value);
        }

        private void PresenceSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            float new_value = (float)e.NewValue;
            Console.WriteLine("Setting new presence value {0}", new_value);
            modulate.set_presence_strength(new_value);
        }

        private void BassBoosterSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            float new_value = (float)e.NewValue;
            Console.WriteLine("Setting new bass booster value {0}", new_value);
            modulate.set_bass_booster_strength(new_value);
        }

        private void IntimidatorSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            float new_value = (float)e.NewValue;
            Console.WriteLine("Setting new intimidator value {0}", new_value);
            modulate.set_intimidator_strength(new_value);
        }

        private void HelmSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            float new_value = (float)e.NewValue;
            Console.WriteLine("Setting new helm value {0}", new_value);
            modulate.set_helm_strength(new_value);
        }

        private void VividSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            float new_value = (float)e.NewValue;
            Console.WriteLine("Setting new vivid value {0}", new_value);
            modulate.set_vivid_strength(new_value);
        }

        private long calculate_logdir_size()
        {
            long size = 0;
            DirectoryInfo dir_info = new DirectoryInfo(modulate_log_folder);
            FileInfo[] files = dir_info.GetFiles();
            foreach (FileInfo fi in files)
                size += fi.Length;
            return size;
        }

        private void update_log_size_text(Object stateinfo)
        {
            long size = calculate_logdir_size();
            long mbs = size / 1000000;
            string text = "Log Size: " + mbs.ToString() + " MB";
            Console.WriteLine(text);
            Dispatcher.Invoke((Action)delegate () { LogSizeTextBlock.Text = text; });
        }

        private void OpenLogDirButton_Click(object sender, RoutedEventArgs e)
        {
            System.Diagnostics.Process.Start(modulate_log_folder);
        }

        private void ClearOldLogsButton_Click(object sender, RoutedEventArgs e)
        {
            long max_age = 3600; // in seconds
            DirectoryInfo dir_info = new DirectoryInfo(modulate_log_folder);
            FileInfo[] files = dir_info.GetFiles();
            DateTime now = DateTime.Now;
            foreach (FileInfo fi in files)
            {
                DateTime last_write_time = fi.LastWriteTime;
                TimeSpan age = now - last_write_time;
                if (age.TotalSeconds > max_age)
                {
                    Console.WriteLine("Deleting file {0} with age {1}", fi.Name, age.TotalSeconds);
                    fi.Delete();
                }
            }
            update_log_size_text(null);
        }
    }
}
