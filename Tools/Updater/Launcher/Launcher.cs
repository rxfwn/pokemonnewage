// Launcher du jeu Pokémon : Pokemon.exe (docs/MISES_A_JOUR.md)
//
// À chaque ouverture : demande au Raspberry Pi la dernière version, télécharge seulement les fichiers
// qui ont changé, puis lance le jeu. Sans connexion, lance la version déjà installée.
//
// Compilé par compiler_launcher.bat avec le compilateur C# fourni dans Windows (.NET Framework 4),
// donc rien à installer. Ce compilateur ne connaît que le C# 5 : pas de $"...", ni de ?. ni de nameof.
//
// Dossier du joueur :
//   Pokemon.exe
//   launcher_config.json    {"server": "http://100.x.x.x:8788"} (demandé au premier lancement)
//   Jeu\                    le jeu, rempli par le launcher

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

[assembly: AssemblyTitle("Pokémon")]
[assembly: AssemblyProduct("Pokémon")]
[assembly: AssemblyDescription("Met le jeu à jour puis le lance")]
[assembly: AssemblyVersion("1.0.0.0")]

namespace PokemonLauncher
{
    class FileEntry
    {
        public long Size;
        public long Mtime;
        public string Sha256;
    }

    class LauncherState
    {
        public string Version = "";
        public bool Complete;
        public string Exe = "";
        public Dictionary<string, FileEntry> Files = new Dictionary<string, FileEntry>();
    }

    class ManifestFile
    {
        public string Path;
        public long Size;
        public string Sha256;
    }

    static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new LauncherForm(args));
        }
    }

    class LauncherForm : Form
    {
        const int Chunk = 1024 * 1024;

        readonly string[] gameArgs;
        readonly string here;
        readonly string gameDir;
        readonly string statePath;
        readonly string configPath;
        readonly JavaScriptSerializer json = new JavaScriptSerializer();

        readonly Label titleLabel = new Label();
        readonly Label statusLabel = new Label();
        readonly Label detailLabel = new Label();
        readonly ProgressBar bar = new ProgressBar();
        readonly Button retryButton = new Button();
        readonly Button playButton = new Button();
        readonly Button closeButton = new Button();

        string server = "";
        bool installedUsable;

        public LauncherForm(string[] args)
        {
            gameArgs = args;
            here = AppDomain.CurrentDomain.BaseDirectory;
            gameDir = Path.Combine(here, "Jeu");
            statePath = Path.Combine(gameDir, ".launcher_etat.json");
            configPath = Path.Combine(here, "launcher_config.json");
            json.MaxJsonLength = int.MaxValue;

            Text = "Pokémon";
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(520, 190);
            BackColor = Color.FromArgb(24, 28, 36);
            ForeColor = Color.White;
            Font = new Font("Segoe UI", 10f);

            titleLabel.Text = "Pokémon";
            titleLabel.Font = new Font("Segoe UI Semibold", 16f);
            titleLabel.ForeColor = Color.FromArgb(232, 72, 72);
            titleLabel.SetBounds(20, 14, 480, 34);

            statusLabel.Text = "Recherche d'une mise à jour...";
            statusLabel.SetBounds(20, 54, 480, 24);

            detailLabel.ForeColor = Color.FromArgb(150, 162, 178);
            detailLabel.Font = new Font("Segoe UI", 9f);
            detailLabel.SetBounds(20, 80, 480, 20);
            detailLabel.AutoEllipsis = true;

            bar.SetBounds(20, 108, 480, 18);
            bar.Maximum = 1000;
            bar.Style = ProgressBarStyle.Marquee;

            SetupButton(retryButton, "Réessayer", 160);
            SetupButton(playButton, "Jouer quand même", 290);
            SetupButton(closeButton, "Fermer", 420);
            playButton.Width = 120;
            retryButton.Click += delegate { HideButtons(); StartWork(); };
            playButton.Click += delegate { LaunchGame(LoadState(), true); };
            closeButton.Click += delegate { Close(); };

            Controls.AddRange(new Control[] { titleLabel, statusLabel, detailLabel, bar, retryButton, playButton, closeButton });
            Shown += delegate { StartWork(); };
        }

        void SetupButton(Button button, string text, int x)
        {
            button.Text = text;
            button.SetBounds(x, 144, 100, 30);
            button.FlatStyle = FlatStyle.Flat;
            button.BackColor = Color.FromArgb(44, 50, 62);
            button.Visible = false;
        }

        void HideButtons()
        {
            retryButton.Visible = playButton.Visible = closeButton.Visible = false;
        }

        // ------------------------------------------------------------ Affichage (depuis le fil de travail)

        void Ui(Action action)
        {
            if (IsDisposed) return;
            if (InvokeRequired) { try { BeginInvoke(action); } catch (Exception) { /* fenêtre fermée */ } }
            else action();
        }

        void Status(string text, string detail)
        {
            Ui(delegate { statusLabel.ForeColor = Color.White; statusLabel.Text = text; detailLabel.Text = detail ?? ""; });
        }

        void Progress(double fraction)
        {
            Ui(delegate
            {
                bar.Style = ProgressBarStyle.Continuous;
                bar.Value = Math.Max(0, Math.Min(1000, (int)(fraction * 1000)));
            });
        }

        void Fail(string text, bool canPlay)
        {
            Ui(delegate
            {
                statusLabel.ForeColor = Color.FromArgb(255, 120, 110);
                statusLabel.Text = "Problème";
                detailLabel.Text = "";
                bar.Style = ProgressBarStyle.Continuous;
                bar.Value = 0;
                retryButton.Visible = true;
                playButton.Visible = canPlay;
                closeButton.Visible = true;
                MessageBox.Show(this, text, "Pokémon", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            });
        }

        static string FormatSize(double bytes)
        {
            if (bytes >= 1024.0 * 1024 * 1024) return (bytes / (1024.0 * 1024 * 1024)).ToString("0.00") + " Go";
            return (bytes / (1024.0 * 1024)).ToString("0") + " Mo";
        }

        // ------------------------------------------------------------ Réglages et état

        bool LoadServer()
        {
            string address = "";
            if (File.Exists(configPath))
            {
                try
                {
                    var config = json.Deserialize<Dictionary<string, object>>(File.ReadAllText(configPath, Encoding.UTF8));
                    object value;
                    if (config != null && config.TryGetValue("server", out value) && value != null) address = value.ToString();
                }
                catch (Exception) { }
            }
            if (address.Length == 0 || address.Contains("x.x"))
            {
                string typed = null;
                Invoke((Action)delegate { typed = AskAddress(); });
                if (string.IsNullOrEmpty(typed)) return false;
                address = typed.Trim();
                if (!address.Contains("://")) address = "http://" + address;
                if (address.IndexOf(':', address.IndexOf("://") + 3) < 0) address += ":8788";
                var config = new Dictionary<string, object>();
                config["server"] = address;
                File.WriteAllText(configPath, json.Serialize(config), new UTF8Encoding(false));
            }
            if (!address.Contains("://")) address = "http://" + address;
            server = address.TrimEnd('/');
            return true;
        }

        string AskAddress()
        {
            using (var dialog = new Form())
            {
                dialog.Text = "Pokémon - premier lancement";
                dialog.FormBorderStyle = FormBorderStyle.FixedDialog;
                dialog.StartPosition = FormStartPosition.CenterParent;
                dialog.MaximizeBox = dialog.MinimizeBox = false;
                dialog.ClientSize = new Size(440, 150);
                dialog.Font = Font;
                var label = new Label();
                label.Text = "Adresse Tailscale du Raspberry Pi (celle qui commence par 100.) :";
                label.SetBounds(16, 16, 410, 40);
                var box = new TextBox();
                box.Text = "100.";
                box.SetBounds(16, 60, 408, 26);
                var ok = new Button();
                ok.Text = "OK";
                ok.DialogResult = DialogResult.OK;
                ok.SetBounds(244, 104, 86, 30);
                var cancel = new Button();
                cancel.Text = "Annuler";
                cancel.DialogResult = DialogResult.Cancel;
                cancel.SetBounds(338, 104, 86, 30);
                dialog.Controls.AddRange(new Control[] { label, box, ok, cancel });
                dialog.AcceptButton = ok;
                dialog.CancelButton = cancel;
                return dialog.ShowDialog(this) == DialogResult.OK ? box.Text : null;
            }
        }

        LauncherState LoadState()
        {
            var state = new LauncherState();
            if (!File.Exists(statePath)) return state;
            try
            {
                var root = json.Deserialize<Dictionary<string, object>>(File.ReadAllText(statePath, Encoding.UTF8));
                state.Version = Convert.ToString(root["version"]);
                state.Complete = Convert.ToBoolean(root["complete"]);
                state.Exe = Convert.ToString(root["exe"]);
                var files = (Dictionary<string, object>)root["files"];
                foreach (var pair in files)
                {
                    var info = (Dictionary<string, object>)pair.Value;
                    var entry = new FileEntry();
                    entry.Size = Convert.ToInt64(info["size"]);
                    entry.Mtime = Convert.ToInt64(info["mtime"]);
                    entry.Sha256 = Convert.ToString(info["sha256"]);
                    state.Files[pair.Key] = entry;
                }
            }
            catch (Exception)
            {
                // État illisible : tous les fichiers seront revérifiés
                return new LauncherState();
            }
            return state;
        }

        void SaveState(LauncherState state)
        {
            Directory.CreateDirectory(gameDir);
            var files = new Dictionary<string, object>();
            foreach (var pair in state.Files)
            {
                var info = new Dictionary<string, object>();
                info["size"] = pair.Value.Size;
                info["mtime"] = pair.Value.Mtime;
                info["sha256"] = pair.Value.Sha256;
                files[pair.Key] = info;
            }
            var root = new Dictionary<string, object>();
            root["version"] = state.Version;
            root["complete"] = state.Complete;
            root["exe"] = state.Exe;
            root["files"] = files;
            string tmp = statePath + ".tmp";
            File.WriteAllText(tmp, json.Serialize(root), new UTF8Encoding(false));
            if (File.Exists(statePath)) File.Delete(statePath);
            File.Move(tmp, statePath);
        }

        string LocalPath(string rel)
        {
            return Path.Combine(gameDir, rel.Replace('/', '\\'));
        }

        static string Sha256Of(string path)
        {
            using (var sha = SHA256.Create())
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, Chunk))
            {
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
            }
        }

        // ------------------------------------------------------------ Lancement du jeu

        void LaunchGame(LauncherState state, bool fromButton)
        {
            string exe = string.IsNullOrEmpty(state.Exe) ? "" : LocalPath(state.Exe);
            if (exe.Length == 0 || !File.Exists(exe))
            {
                if (fromButton) MessageBox.Show(this, "Le jeu n'est pas encore installé.", "Pokémon");
                else Fail("Le jeu n'est pas encore installé et le serveur est injoignable.\n\nVérifie que Tailscale est connecté, puis clique sur Réessayer.", false);
                return;
            }
            var start = new ProcessStartInfo(exe);
            start.WorkingDirectory = gameDir;
            start.UseShellExecute = true;
            var quoted = new List<string>();
            foreach (string arg in gameArgs) quoted.Add(arg.Contains(" ") ? "\"" + arg + "\"" : arg);
            start.Arguments = string.Join(" ", quoted.ToArray());
            try
            {
                Process.Start(start);
            }
            catch (Exception e)
            {
                Fail("Impossible de lancer le jeu : " + e.Message, false);
                return;
            }
            Ui(delegate { Close(); });
        }

        // ------------------------------------------------------------ Mise à jour

        void StartWork()
        {
            bar.Style = ProgressBarStyle.Marquee;
            Status("Recherche d'une mise à jour...", "");
            var thread = new Thread(delegate ()
            {
                try { Work(); }
                catch (Exception e)
                {
                    Fail("La mise à jour s'est arrêtée : " + e.Message + "\n\nClique sur Réessayer : elle reprendra là où elle s'est arrêtée.", installedUsable);
                }
            });
            thread.IsBackground = true;
            thread.Start();
        }

        HttpWebRequest Request(string path, int timeoutMs)
        {
            var request = (HttpWebRequest)WebRequest.Create(server + path);
            request.Proxy = null;              // évite d'attendre la détection de proxy de Windows
            request.Timeout = timeoutMs;
            request.ReadWriteTimeout = 30000;  // le Pi ne répond plus pendant 30 s : on réessaie
            return request;
        }

        void Work()
        {
            if (!LoadServer())
            {
                Ui(delegate { Close(); });
                return;
            }
            LauncherState state = LoadState();
            installedUsable = state.Complete && !string.IsNullOrEmpty(state.Exe) && File.Exists(LocalPath(state.Exe));

            // Dernière version sur le Pi
            Dictionary<string, object> manifest;
            try
            {
                using (var response = Request("/manifest", 6000).GetResponse())
                using (var reader = new StreamReader(response.GetResponseStream(), Encoding.UTF8))
                {
                    manifest = json.Deserialize<Dictionary<string, object>>(reader.ReadToEnd());
                }
            }
            catch (Exception)
            {
                if (installedUsable)
                {
                    Status("Serveur injoignable : lancement de la version installée...", state.Version);
                    Thread.Sleep(1200);
                    LaunchGame(state, false);
                }
                else if (!string.IsNullOrEmpty(state.Version) && !state.Complete)
                {
                    Fail("La dernière mise à jour n'est pas finie et le serveur est injoignable.\n\nConnecte Tailscale, puis clique sur Réessayer.", false);
                }
                else
                {
                    LaunchGame(state, false);
                }
                return;
            }

            string version = Convert.ToString(manifest["version"]);
            string note = manifest.ContainsKey("note") ? Convert.ToString(manifest["note"]) : "";
            string exe = Convert.ToString(manifest["exe"]);
            Ui(delegate { titleLabel.Text = "Pokémon  " + version + (note.Length > 0 ? "  (" + note + ")" : ""); });

            var wanted = new Dictionary<string, ManifestFile>();
            foreach (var pair in (Dictionary<string, object>)manifest["files"])
            {
                var info = (Dictionary<string, object>)pair.Value;
                var file = new ManifestFile();
                file.Path = pair.Key;
                file.Size = Convert.ToInt64(info["size"]);
                file.Sha256 = Convert.ToString(info["sha256"]);
                if (file.Path.Contains("..") || Path.IsPathRooted(file.Path.Replace('/', '\\')))
                {
                    throw new InvalidDataException("chemin refusé dans la liste du serveur : " + file.Path);
                }
                wanted[pair.Key] = file;
            }

            // Quels fichiers ont changé ?
            var toGet = new List<ManifestFile>();
            var newFiles = new Dictionary<string, FileEntry>();
            int checkedCount = 0;
            foreach (ManifestFile file in wanted.Values)
            {
                checkedCount++;
                string local = LocalPath(file.Path);
                bool ok = false;
                if (File.Exists(local))
                {
                    var item = new FileInfo(local);
                    long mtime = item.LastWriteTimeUtc.Ticks;
                    FileEntry known;
                    string sha = null;
                    if (state.Files.TryGetValue(file.Path, out known) && known.Size == item.Length && known.Mtime == mtime)
                    {
                        // Pas touché depuis la dernière fois : pas besoin de le relire
                        sha = known.Sha256;
                    }
                    else if (item.Length == file.Size)
                    {
                        Status("Vérification des fichiers installés...", file.Path);
                        Progress((double)checkedCount / wanted.Count);
                        sha = Sha256Of(local);
                    }
                    ok = sha == file.Sha256;
                    if (ok)
                    {
                        var entry = new FileEntry();
                        entry.Size = item.Length;
                        entry.Mtime = mtime;
                        entry.Sha256 = sha;
                        newFiles[file.Path] = entry;
                    }
                }
                if (!ok) toGet.Add(file);
            }

            var toDelete = new List<string>();
            foreach (string rel in state.Files.Keys) if (!wanted.ContainsKey(rel)) toDelete.Add(rel);

            if (toGet.Count == 0 && toDelete.Count == 0)
            {
                state.Version = version; state.Exe = exe; state.Complete = true; state.Files = newFiles;
                SaveState(state);
                Status("Le jeu est à jour.", "Lancement...");
                Progress(1);
                LaunchGame(state, false);
                return;
            }

            long total = 0;
            foreach (ManifestFile file in toGet) total += file.Size;
            Directory.CreateDirectory(gameDir);
            var drive = new DriveInfo(Path.GetPathRoot(Path.GetFullPath(gameDir)));
            if (drive.AvailableFreeSpace < total + 500L * 1024 * 1024)
            {
                Fail("Pas assez de place sur le disque " + drive.Name + " : il faut " + FormatSize(total + 500L * 1024 * 1024) + " de libre.", installedUsable);
                return;
            }

            // Téléchargement : chaque fichier va d'abord dans un .part à côté (reprise possible si on ferme),
            // et les anciens fichiers ne sont remplacés qu'une fois TOUT téléchargé et vérifié.
            long done = 0;
            long resumed = 0;
            foreach (ManifestFile file in toGet)
            {
                string part = PartPath(file);
                if (File.Exists(part)) resumed += Math.Min(new FileInfo(part).Length, file.Size);
            }
            var watch = Stopwatch.StartNew();
            var buffer = new byte[Chunk];
            foreach (ManifestFile file in toGet)
            {
                for (int attempt = 1; ; attempt++)
                {
                    try
                    {
                        Download(file, buffer, done, total, resumed, version, watch);
                        break;
                    }
                    catch (Exception e)
                    {
                        if (attempt >= 4) throw;
                        Status("Coupure, nouvel essai dans 3 s...", e.Message);
                        Thread.Sleep(3000);
                    }
                }
                done += file.Size;
            }

            // Remplacement : à partir d'ici, le jeu n'est plus « complet » tant que tout n'est pas remplacé
            Status("Installation...", "");
            state.Complete = false;
            state.Version = version;
            SaveState(state);
            installedUsable = false;
            foreach (ManifestFile file in toGet)
            {
                string local = LocalPath(file.Path);
                try
                {
                    if (File.Exists(local)) File.Delete(local);
                    File.Move(PartPath(file), local);
                }
                catch (Exception)
                {
                    Fail("Impossible de remplacer " + file.Path + " : le jeu est peut-être encore ouvert.\n\nFerme-le, puis clique sur Réessayer.", false);
                    return;
                }
                var item = new FileInfo(local);
                var entry = new FileEntry();
                entry.Size = item.Length;
                entry.Mtime = item.LastWriteTimeUtc.Ticks;
                entry.Sha256 = file.Sha256;
                newFiles[file.Path] = entry;
            }
            foreach (string rel in toDelete)
            {
                try { if (File.Exists(LocalPath(rel))) File.Delete(LocalPath(rel)); } catch (Exception) { }
            }
            // Morceaux de téléchargements abandonnés (une version publiée entre deux essais)
            foreach (string part in Directory.GetFiles(gameDir, "*.part", SearchOption.AllDirectories))
            {
                try { File.Delete(part); } catch (Exception) { }
            }

            state.Version = version; state.Exe = exe; state.Complete = true; state.Files = newFiles;
            SaveState(state);
            Status("Mise à jour terminée.", FormatSize(total) + " en " + watch.Elapsed.TotalSeconds.ToString("0") + " s. Lancement...");
            Progress(1);
            Thread.Sleep(800);
            LaunchGame(state, false);
        }

        string PartPath(ManifestFile file)
        {
            return LocalPath(file.Path) + "." + file.Sha256.Substring(0, 12) + ".part";
        }

        void Download(ManifestFile file, byte[] buffer, long done, long total, long resumed, string version, Stopwatch watch)
        {
            string part = PartPath(file);
            Directory.CreateDirectory(Path.GetDirectoryName(part));
            long start = File.Exists(part) ? new FileInfo(part).Length : 0;
            if (start > file.Size) { File.Delete(part); start = 0; }

            if (start < file.Size || file.Size == 0)
            {
                var request = Request("/blobs/" + file.Sha256, 15000);
                if (start > 0) request.AddRange(start);
                using (var response = (HttpWebResponse)request.GetResponse())
                {
                    FileMode mode;
                    if (response.StatusCode == HttpStatusCode.PartialContent) mode = FileMode.Append;
                    else { mode = FileMode.Create; start = 0; }
                    long current = start;
                    long lastShown = 0;
                    using (Stream input = response.GetResponseStream())
                    using (var output = new FileStream(part, mode, FileAccess.Write, FileShare.None, Chunk))
                    {
                        int n;
                        while ((n = input.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            output.Write(buffer, 0, n);
                            current += n;
                            if (watch.ElapsedMilliseconds - lastShown > 300)
                            {
                                lastShown = watch.ElapsedMilliseconds;
                                long shown = done + current;
                                double speed = (shown - resumed) / Math.Max(0.5, watch.Elapsed.TotalSeconds);
                                string left = speed > 0 ? ", encore " + FormatDuration((total - shown) / speed) : "";
                                Status("Téléchargement de la mise à jour : " + FormatSize(shown) + " / " + FormatSize(total),
                                       (speed / (1024 * 1024)).ToString("0.0") + " Mo/s" + left + "   " + file.Path);
                                Progress(total > 0 ? (double)shown / total : 1);
                            }
                        }
                    }
                }
            }

            Status("Vérification...", file.Path);
            if (Sha256Of(part) != file.Sha256)
            {
                File.Delete(part);
                throw new IOException("fichier abîmé pendant le téléchargement : " + file.Path);
            }
        }

        static string FormatDuration(double seconds)
        {
            if (seconds < 60) return ((int)seconds) + " s";
            if (seconds < 3600) return ((int)(seconds / 60)) + " min";
            return ((int)(seconds / 3600)) + " h " + ((int)(seconds % 3600 / 60)) + " min";
        }
    }
}
