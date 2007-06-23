//
// mopen.cs: Mono Open Application
//
// Opens a XAML file or an application
//
// Author:
//   Miguel de Icaza (miguel@novell.com)
//
//
// See LICENSE file in the Moonlight distribution for licensing details
//

using System;
using Gtk;
using Gtk.Moonlight;
using System.Windows;
using System.Windows.Media;
using System.Windows.Controls;
using System.Windows.Shapes;
using System.Windows.Input;
using System.IO;
using System.Collections;

class MonoOpen {
	static bool fixedwindow = false;
	static int width = -1;
	static int height = -1;
	
	static void Help ()
	{
		Console.WriteLine ("Usage is: mopen [args] [file.xaml|dirname]\n\n" +
				   "Arguments are:\n" +
				   "   --fixed         Disable window resizing\n"  +
				   "   --geometry WxH  Overrides the geometry to be W x H pixels\n" +
				   "   --host NAME     Specifies that this file should be loaded in host NAME\n" 
				   );
	}

	static int LoadXaml (string file, ArrayList args)
	{
		Application.Init ();
		Window w = new Window (file);
		w.DeleteEvent += delegate {
			Application.Quit ();
		};

		GtkSilver silver;
		if (width != -1 && height != -1)
			silver = new GtkSilver (width, height);
		else
			silver = new GtkSilver (100, 100);

		w.SizeAllocated += delegate (object o, SizeAllocatedArgs a){
			silver.SizeAllocate (new Gdk.Rectangle (0, 0, a.Allocation.Width, a.Allocation.Height));
		};

		string xaml = "";
		
		try {
			using (FileStream fs = File.OpenRead (file)){
				using (StreamReader sr = new StreamReader (fs)){
					xaml = sr.ReadToEnd ();
				}
			}
		} catch (Exception e) {
			Console.Error.WriteLine ("mopen: Error loading XAML file {0}: {1}", file, e.GetType());
			return 1;
		}
		
		if (xaml == null){
			Console.Error.WriteLine ("mopen: Error loading XAML file {0}", file);
			return 1;
		}

		Console.WriteLine ("Loading: {0}", xaml);
		DependencyObject d = XamlReader.Load (xaml);
		if (d == null){
			Console.Error.WriteLine ("mopen: No dependency object returned from XamlReader");
			return 1;
		}
		
		w.Add (silver);
		

		if (!(d is Canvas)){
			Console.Error.WriteLine ("mopen: No Canvas as root in the specified file");
			return 1;
		}

		Canvas canvas = d as Canvas;
		if (width != -1 && height != -1){
			w.Resize (width, height);
		} else if (canvas.Width > 0 && canvas.Height > 0)
			w.Resize ((int) canvas.Width, (int) canvas.Height);

		silver.Attach (canvas);

		w.ShowAll ();
		Application.Run ();
		return 0;
	}

	static bool ParseGeometry (string geometry, out int width, out int height)
	{
		// FIXME: implement this
		Console.WriteLine ("ParseGeometry not implemented");
		
		width = 100;
		height = 100;

		return true;
	}
	
	//
	// TODO, if a host is specified, look for it, if not,
	// create the domain ourselves and listen to requests.
	//
	static int DoLoad (string file, ArrayList args)
	{
		if (file.EndsWith (".xaml"))
			return LoadXaml (file, args);

		//
		// Here:
		//    implement loading the DLL or executanle, search in path perhaps?
		//
		if (file.EndsWith (".dll")){
		}

		return 1;
	}
	
	static int Main (string [] args)
	{
		ArrayList cmdargs = new ArrayList ();
		string file = null;
		
		if (args.Length < 1){
			Help ();
			return 1;
		}

		for (int i = 0; i < args.Length; i++){
			switch (args [i]){
			case "-h": case "-help":
				Help ();
				return 0;

			case "-fixed": case "--fixed":
				fixedwindow = true;
				break;

			case "--geometry": case "-g":
				if (i+1 == args.Length){
					Console.Error.WriteLine ("mopen: geometry flag `{0}' takes an argument", args [i]);
					return 1;
				}
				i++;
				if (ParseGeometry (args [i], out width, out height))
					break;
				else
					return 1;

			case "--host":
				if (i+1 == args.Length){
					Console.WriteLine ("mopen: host flag `{0}' takes an argument", args [i]);
					return 1;
				}
				// FIXME: implement this
				Console.WriteLine ("--host not implemented");
				break;
				
			default:
				if (file == null)
					file = args [i];
				else
					cmdargs.Add (args [i]);
				break;
			}
		}

		if (File.Exists (file))
			return DoLoad (file, cmdargs);

		Console.Error.WriteLine ("mopen: Nothing to do");
		return 1;
	}
}
