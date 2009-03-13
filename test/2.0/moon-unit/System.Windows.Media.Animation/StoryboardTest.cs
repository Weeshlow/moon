//
// Unit tests for System.Windows.Media.Animation.Storyboard
//
// Contact:
//   Moonlight List (moonlight-list@lists.ximian.com)
//
// Copyright (C) 2008,2009 Novell, Inc (http://www.novell.com)
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Markup;
using System.Windows.Media.Animation;
using System.Windows.Shapes;

using Microsoft.VisualStudio.TestTools.UnitTesting;
using Mono.Moonlight.UnitTesting;
using Microsoft.Silverlight.Testing;
using System.Threading;
using System.Collections.Generic;
using Microsoft.Silverlight.Testing.UnitTesting;
using System.Windows.Media;

namespace MoonTest.System.Windows.Media.Animation {

	public class Test : UserControl {

		public Storyboard Blah;
	}
	
	[TestClass]
	public class ___StoryboardTest : SilverlightTest {
		
		[TestMethod]
		public void InvalidValues_NonTimeline ()
		{
			Rectangle r = new Rectangle ();
			Assert.Throws<Exception> (delegate {
				r.SetValue (Storyboard.TargetNameProperty, null);
			}, "#1");
			Assert.Throws<Exception> (delegate {
				r.SetValue (Storyboard.TargetNameProperty, "");
			}, "#1b");
			Assert.Throws<Exception> (delegate {
				r.SetValue (Storyboard.TargetNameProperty, "X");
			}, "#1c");
			Assert.Throws<Exception> (delegate {
				r.SetValue (Storyboard.TargetPropertyProperty, null);
			}, "2");
		}

		[TestMethod]
		public void ValidValues_Timeline ()
		{
			Timeline t = (Timeline) new ColorAnimation ();
			t.SetValue (Storyboard.TargetNameProperty, null);
			t.SetValue (Storyboard.TargetNameProperty, String.Empty);
			t.SetValue (Storyboard.TargetNameProperty, "X");
			t.SetValue (Storyboard.TargetPropertyProperty, null);
		}

		[TestMethod]
		[Asynchronous]
		public void MultipleComplete ()
		{
			int count = 0;

			Rectangle target = new Rectangle { Width = 0 };
			Storyboard a = new Storyboard ();
			DoubleAnimation animation = new DoubleAnimation { By = 10, Duration = new Duration (TimeSpan.FromMilliseconds (100)) };
			Storyboard.SetTarget (animation, target);
			Storyboard.SetTargetProperty (animation, new PropertyPath ("Width"));
			a.Children.Add (animation);

			a.Completed += delegate { count++; a.Begin (); };

			Enqueue (() => TestPanel.Children.Add (target));
			Enqueue (() => a.Begin ());
			EnqueueConditional (() => count == 5, TimeSpan.FromMilliseconds (2000));
			
			// We should move exactly 50 pixels but can overshoot the target by > 10%.
			Enqueue (() => Assert.IsBetween (40, 60, target.Width, "#1"));
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void MultipleStartStop ()
		{
			int count = 0;
			
			Rectangle target = new Rectangle ();
			
			Storyboard a = new Storyboard ();
			DoubleAnimation animation = new DoubleAnimation { From = 5, To = 100, Duration = new Duration (TimeSpan.FromMilliseconds (100)) };
			Storyboard.SetTarget (animation, target);
			Storyboard.SetTargetProperty (animation, new PropertyPath ("Width"));
			a.Children.Add (animation);

			a.Completed += delegate { count++; a.Begin (); };

			Enqueue (() => TestPanel.Children.Add (target));
			Enqueue (() => a.Begin ());
			EnqueueConditional (() => count == 5, TimeSpan.FromMilliseconds (2000));
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void MultipleStartStop2 ()
		{
			int count = 0;

			Rectangle target = new Rectangle ();

			Storyboard a = new Storyboard ();
			DoubleAnimation animation = new DoubleAnimation { From = 5, To = 100, Duration = new Duration (TimeSpan.FromMilliseconds (100)) };
			Storyboard.SetTarget (animation, target);
			Storyboard.SetTargetProperty (animation, new PropertyPath ("Width"));
			a.Children.Add (animation);

			a.Completed += delegate { count++; a.Begin (); };

			Enqueue (() => TestPanel.Children.Add (target));
			//Enqueue (() => TestPanel.Resources.Add ("SB", a));
			Enqueue (() => a.Begin ());
			EnqueueConditional (() => count == 5, TimeSpan.FromMilliseconds (1000));
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void NotAttached ()
		{
			int count = 0;

			Rectangle target = new Rectangle ();

			Storyboard a = new Storyboard ();
			DoubleAnimation animation = new DoubleAnimation { From = 5, To = 100, Duration = new Duration (TimeSpan.FromMilliseconds (100)) };
			Storyboard.SetTarget (animation, target);
			Storyboard.SetTargetProperty (animation, new PropertyPath ("Width"));
			a.Children.Add (animation);

			a.Completed += delegate { Console.WriteLine ("Completed: {0}", count+1); count++; if(count != 5) a.Begin (); };

			Enqueue (() => {Console.WriteLine ("Starting our one"); a.Begin (); });
			EnqueueConditional (() => count == 5, TimeSpan.FromMilliseconds (1500));
			Enqueue (() => Assert.AreEqual (100, target.Width, "#1"));
			EnqueueTestComplete ();
		}


		[TestMethod]
		[Asynchronous]
		public void PauseStoryboard ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];
			Storyboard child = (Storyboard) sb.Children [1];
			TestPanel.Children.Add (c);
			((Rectangle) c.Children [0]).Width = 0;
			int start = 0;

			Enqueue (() => { sb.Begin (); start = Environment.TickCount; });
			EnqueueConditional (() => Environment.TickCount - start > 100, "#0");
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => child.Stop (), "#1"));
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => child.Pause (), "#2"));
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => child.Seek (TimeSpan.FromMilliseconds (10)), "#3"));
			Enqueue (() => Assert.IsGreater (15, ((Rectangle) c.Children [0]).Width, "#5"));
			Enqueue (() => sb.Pause ());
			Enqueue (() => Assert.IsGreater (15, ((Rectangle) c.Children [0]).Width, "#6"));
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => child.Resume (), "#4"));
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		public void NameAndKey()
		{
			Storyboard board = (Storyboard)XamlReader.Load(
@"		
<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
            xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml""
            x:Name=""Blah""
            x:Key=""Blah"" />
");
			Assert.AreEqual("Blah", board.GetValue(FrameworkElement.NameProperty), "#1");
			board = (Storyboard)XamlReader.Load(
@"	
<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
            xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml""
            x:Name=""Blah2""
            x:Key=""Blah"" />
");
			Assert.AreEqual("Blah2", board.GetValue(FrameworkElement.NameProperty), "#1");
			board = (Storyboard)XamlReader.Load(
@"	
<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
            xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml""
            x:Key=""Blah""
            x:Name=""Blah2"" />
");
			Assert.AreEqual("Blah2", board.GetValue(FrameworkElement.NameProperty), "#1");
		}

		[TestMethod]
		public void NameAndKey_Resource ()
		{
			Canvas c = (Canvas) XamlReader.Load (
@"<Canvas xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation"" xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<Canvas.Resources>
		<Storyboard x:Name=""Blah"" x:Key=""Blah"" />
	</Canvas.Resources>
</Canvas>");
			Assert.IsTrue (c.FindName ("Blah") is Storyboard, "FindName");

			c = (Canvas) XamlReader.Load (
@"<Canvas xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation"" xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<Canvas.Resources>
		<Storyboard x:Name=""Blah1"" x:Key=""Blah2"" />
	</Canvas.Resources>
</Canvas>");
			Assert.IsTrue (c.FindName ("Blah1") is Storyboard, "FindName-Name");
			Assert.IsNull (c.FindName ("Blah2"), "FindName-Key");
		}

		[TestMethod]
		public void NameAndKey_Resource_Namespace ()
		{
			Canvas c = (Canvas) XamlReader.Load (
@"<Canvas xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation"" xmlns:sl=""http://schemas.microsoft.com/winfx/2006/xaml/presentation"">
	<sl:Canvas xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation"">
		<sl:Canvas.Resources xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
			<sl:Storyboard x:Name=""Blah"" x:Key=""Blah"" />
		</sl:Canvas.Resources>
	</sl:Canvas>
</Canvas>");
			Assert.IsTrue (c.FindName ("Blah") is Storyboard, "FindName");
		}

		[TestMethod]
		public void SetTarget ()
		{
			Rectangle r = new Rectangle ();
			Assert.Throws<ArgumentNullException> (delegate {
				Storyboard.SetTarget (null, r);
			}, "null, do");

			Timeline t = (Timeline) new ColorAnimation ();
			Assert.Throws<ArgumentNullException> (delegate {
				Storyboard.SetTarget (t, null);
			}, "timeline, null");
		}
		
		[TestMethod]
		[Asynchronous]
		public void SetTargetAndName ()
		{
			Rectangle target = new Rectangle { Width = 0 };
			Rectangle rect = new Rectangle { Width = 0, Name = "rect" };

			Storyboard sb = new Storyboard ();
			DoubleAnimation anim = new DoubleAnimation { From = 10, To = 100, Duration = TimeSpan.FromMilliseconds (100) };
			sb.Children.Add (anim);

			Storyboard.SetTargetName (anim, "rect");
			Storyboard.SetTarget (anim, target);
			Storyboard.SetTargetProperty (anim, new PropertyPath (Rectangle.WidthProperty));

			Enqueue (() => {
				TestPanel.Children.Add (target);
				TestPanel.Children.Add (rect);
				TestPanel.Resources.Add ("a", sb);
			});
			Enqueue (() => sb.Begin ());
			Enqueue (() => Assert.IsGreater (0, target.Width, "#1"));
			Enqueue (() => Assert.AreEqual (0, rect.Width, "#2"));
			Enqueue (() => {
				TestPanel.Children.Clear ();
				TestPanel.Resources.Clear ();
			});
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		[MoonlightBug]
		public void SetTargetPropertyTwice ()
		{
			Rectangle target = new Rectangle { Width = 0, Height = 0 };

			Storyboard sb = new Storyboard ();
			DoubleAnimation anim = new DoubleAnimation { From = 10, To = 100, Duration = TimeSpan.FromMilliseconds (100) };
			sb.Children.Add (anim);

			Storyboard.SetTarget (anim, target);

			Enqueue (() => {
				Storyboard.SetTargetProperty (anim, new PropertyPath (Rectangle.WidthProperty));
				Assert.IsNull (Storyboard.GetTargetProperty (anim), "Null target");
			});
			Enqueue (() => {
				Storyboard.SetTargetProperty (anim, new PropertyPath ("Height"));
				Assert.IsNotNull (Storyboard.GetTargetProperty (anim), "Not null target");
			});
			Enqueue (() => {
				TestPanel.Children.Add (target);
				TestPanel.Resources.Add ("a", sb);
			});
			Enqueue (() => sb.Begin ());
			Enqueue (() => Assert.AreEqual (0, target.Height, "#1"));
			Enqueue (() => Assert.IsGreater (0, target.Width, "#2"));
			Enqueue (() => {
				TestPanel.Children.Clear ();
				TestPanel.Resources.Clear ();
			});
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		[MoonlightBug]
		public void SetTargetPropertyTwice2 ()
		{
			Rectangle target = new Rectangle { Width = 0, Height = 0 };

			Storyboard sb = new Storyboard ();
			DoubleAnimation anim = new DoubleAnimation { From = 10, To = 100, Duration = TimeSpan.FromMilliseconds (100) };
			sb.Children.Add (anim);

			Storyboard.SetTarget (anim, target);
			Storyboard.SetTargetProperty (anim, new PropertyPath (Rectangle.WidthProperty));
			Storyboard.SetTargetProperty (anim, new PropertyPath (Rectangle.HeightProperty));
			Enqueue (() => {
				TestPanel.Children.Add (target);
				TestPanel.Resources.Add ("a", sb);
			});
			Enqueue (() => sb.Begin ());
			Enqueue (() => Assert.IsGreater (0, target.Height, "#1"));
			Enqueue (() => Assert.AreEqual (0, target.Width, "#2"));
			Enqueue (() => {
				TestPanel.Children.Clear ();
				TestPanel.Resources.Clear ();
			});
			EnqueueTestComplete ();
		}

		[TestMethod]
		public void SetTargetName ()
		{
			Assert.Throws<NullReferenceException> (delegate {
				Storyboard.SetTargetName (null, "moon");
			}, "null, string");
		}

		[TestMethod]
		public void SetTargetName_NullName ()
		{
			Timeline t = (Timeline) new ColorAnimation ();
			Assert.IsNull (t.GetValue (Storyboard.TargetNameProperty), "GetValue(TargetNameProperty)");
			Storyboard.SetTargetName (t, null);
			// and the behavior is not specific to Storyboard.SetTargetName
			t.SetValue (Storyboard.TargetNameProperty, (string)null);

			// it's even reset-able to null after a value is assigned
			Storyboard.SetTargetName (t, "uho");
			Storyboard.SetTargetName (t, null);
			Assert.IsNull (Storyboard.GetTargetName (t), "GetTargetName-final");
			Assert.IsNull (t.GetValue (Storyboard.TargetNameProperty), "GetValue(TargetNameProperty)-final");
		}

		[TestMethod]
		public void SetTargetProperty ()
		{
			PropertyPath pp = new PropertyPath ("/moon");
			Assert.Throws<ArgumentNullException> (delegate {
				Storyboard.SetTargetProperty (null, pp);
			}, "null, PropertyPath");

			Timeline t = (Timeline) new ColorAnimation ();
			Assert.Throws<ArgumentNullException> (delegate {
				Storyboard.SetTargetProperty (t, null);
			}, "timeline, null");
		}

		[TestMethod]
		public void GetTargetName ()
		{
			Assert.Throws<NullReferenceException> (delegate {
				Storyboard.GetTargetName (null);
			}, "null");

			Timeline t = (Timeline) new ColorAnimation ();
			Assert.IsNull (Storyboard.GetTargetName (t), "GetTargetName(ColorAnimation)");
		}

		[TestMethod]
		public void GetTargetProperty ()
		{
			Assert.Throws<ArgumentNullException> (delegate {
				Storyboard.GetTargetProperty (null);
			}, "null");
		}

		[TestMethod]
		[Asynchronous]
		public void AddRunningStoryboard ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];

			Storyboard child = new Storyboard { };
			DoubleAnimation animation = new DoubleAnimation { Duration = new Duration (TimeSpan.FromSeconds (1)), From = 10, To = 100 };
			Storyboard.SetTargetName (animation, "A");
			Storyboard.SetTargetProperty (animation, new PropertyPath ("Width"));
			child.Children.Add (animation);

			TestPanel.Children.Add (c);

			Enqueue (delegate {
				// The TargetName isn't resolvable
				Assert.Throws<InvalidOperationException>(() => child.Begin (), "TargetName should not be resolvable");
				Storyboard.SetTargetName (animation, null);
				Assert.Throws<InvalidOperationException> (() => child.Begin (), "Target is not specified");
				Storyboard.SetTarget (child, c.Children [0]);
				child.Begin ();
			});

			Enqueue (delegate {
				Assert.AreEqual (ClockState.Active, child.GetCurrentState (), "Active");
				sb.Children.Add (child);
			});

			Enqueue (delegate {
				Assert.AreEqual (ClockState.Active, child.GetCurrentState (), "#1");
				Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState (), "#2");
			});

			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void RemoveAnimationWhileRunning ()
		{
			int start = Environment.TickCount;
			double width = 0;
			Rectangle target = new Rectangle { Fill = new SolidColorBrush (Colors.Red) };

			Storyboard sb = new Storyboard ();
			DoubleAnimation animation = new DoubleAnimation { From = 5, To = 100, Duration = new Duration (TimeSpan.FromMilliseconds (1000)) };
			Storyboard.SetTarget (animation, target);
			Storyboard.SetTargetProperty (animation, new PropertyPath ("Width"));
			sb.Children.Add (animation);

			Enqueue (() => TestPanel.Children.Add (target));
			Enqueue (() => TestPanel.Resources.Add ("SB", sb));

			Enqueue (() => {
				sb.Begin ();
				start = Environment.TickCount;
			});

			EnqueueConditional (() => Environment.TickCount - start > 100, "100ms");
			Enqueue (() => {
				width = target.Width;
				Assert.IsGreater(5, width, "1");
				TestPanel.Resources.Clear();
				start = Environment.TickCount;
			});

			EnqueueConditional(() => Environment.TickCount - start > 300, "300ms");
			Enqueue (() => {
				Assert.IsGreater (width + 10, target.Width, "2");
			});
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		public void RemoveChildThenStart ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];
			Storyboard child = (Storyboard) sb.Children [1];
			sb.Children.RemoveAt (1);
			c.Resources.Clear();
			c.Resources.Add ("asdasd", child);
			CreateAsyncTest (c, () =>
				Assert.Throws<InvalidOperationException> (() => child.Begin ())
			);
		}

		[TestMethod]
		[Asynchronous]
		public void RemoveChildThenStart2 ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];
			Storyboard child = (Storyboard) sb.Children [1];
			sb.Children.RemoveAt (1);
			c.Resources.Clear ();

			Storyboard storyboard = new Storyboard ();
			storyboard.Children.Add (child);

			c.Resources.Add ("asdasd", storyboard);
			CreateAsyncTest (c,
				() => storyboard.Begin (),
				() => storyboard.Stop ()
			);
		}

		[TestMethod]
		[Asynchronous]
		public void RemoveChildThenStart3 ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard root = (Storyboard) c.Resources ["Storyboard"];
			Timeline timeline = ((Storyboard) root.Children [0]).Children [0];
			((Storyboard)root.Children[0]).Children.RemoveAt (0);
			c.Resources.Clear ();
			Storyboard storyboard = new Storyboard ();
			storyboard.Children.Add (timeline);

			c.Resources.Add ("asdasd", storyboard);
			CreateAsyncTest (c,
				() => storyboard.Begin (),
				() => storyboard.Stop ()
			);
		}

		[TestMethod]
		[Asynchronous]
		[MoonlightBug]
		public void RemoveChildThenStart4 ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];
			Storyboard child = (Storyboard) sb.Children [1];
			sb.Children.RemoveAt (1);
			c.Resources.Clear ();

			TimelineCollection collection = new TimelineCollection();
			Storyboard storyboard = new Storyboard ();
			storyboard.Children.Add (child);
			Storyboard.SetTargetName (child, null);
			Storyboard.SetTarget (child, c.Children[0]);

			CreateAsyncTest (c,
				delegate { storyboard.Begin (); },
				delegate { storyboard.Stop (); },
				delegate { collection.Add (storyboard); },
				delegate { storyboard.Begin (); },
				delegate { storyboard.Stop (); },
				delegate { collection.Remove (storyboard); },
				delegate { sb.Children.Add (storyboard); },
				delegate { sb.Children.Remove (storyboard); },
				delegate { Assert.Throws<InvalidOperationException> (delegate { storyboard.Begin (); }); }
			);
		}
		
		// If this test executes before BeginParentHasNameAttached2, then BeginParentHasNameAttached2
		// will fail with a "cannot find 'rect'" error even though rect should definitely be findable.
		// Looks like a SL bug.
		[TestMethod]
		[Asynchronous]
		public void zBeginInvalidTargetProperty ()
		{
			Rectangle target = new Rectangle { Name = "TA" };
			Storyboard sb = new Storyboard ();
			sb.Children.Add (new DoubleAnimation { From = 5, To = 100 });
			Storyboard.SetTargetName (sb, "TA");
			Storyboard.SetTargetProperty (sb, new PropertyPath ("FakeProp"));

			Enqueue (() => { TestPanel.Children.Add (target); TestPanel.Resources.Add ("b", sb); });
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => sb.Begin ()));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}


		[TestMethod]
		public void BeginNoNameOrTarget ()
		{
			Storyboard sb = new Storyboard ();
			sb.Begin ();
			Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState ());
		}

		[TestMethod]
		public void BeginNoNameOrTarget2 ()
		{
			Assert.Throws <InvalidOperationException> (() => {
				Storyboard sb = new Storyboard ();
				sb.Children.Add (new DoubleAnimation { From = 5, To = 100 });
				sb.Begin ();
			});
		}

		[TestMethod]
		public void BeginParentHasNameNotAttached ()
		{
			Assert.Throws <InvalidOperationException> (() => {
				Storyboard sb = new Storyboard ();
				Storyboard.SetTargetName (sb, "TARGET");
				sb.Children.Add (new DoubleAnimation { From = 5, To = 100 });
				sb.Begin ();
			});
		}

		[TestMethod]
		[Asynchronous]
		public void BeginParentHasNameAttached ()
		{
			Rectangle target = new Rectangle { Name = "TARGET" };
			Storyboard sb = new Storyboard ();
			sb.Children.Add (new DoubleAnimation { From = 5, To = 100 });
			Storyboard.SetTargetName (sb, "TARGET");
			Storyboard.SetTargetProperty (sb, new PropertyPath ("Width"));

			Enqueue (() => TestPanel.Children.Add (target));
			Enqueue (() => Assert.Throws <InvalidOperationException> (() => sb.Begin ()));
			Enqueue (() => TestPanel.Children.Clear ());
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		public void BeginParentHasNameAttached2 ()
		{
			Rectangle target = new Rectangle { Name = "rect" };
			Storyboard sb = new Storyboard ();
			sb.Children.Add (new DoubleAnimation { From = 5, To = 100 });
			Storyboard.SetTargetName (sb, "rect");
			Storyboard.SetTargetProperty (sb, new PropertyPath ("Width"));

			Enqueue (() => { TestPanel.Children.Add (target); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			Enqueue (() => Assert.AreNotEqual (target.Width, 0));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}

		[TestMethod]
		public void BeginNameNotAttached2 ()
		{
			Storyboard sb = new Storyboard ();
			Storyboard.SetTargetName (sb, "TARGET");
			sb.Begin ();
			Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState ());
		}

		[TestMethod]
		[Asynchronous]
		[MoonlightBug]
		public void CurrentState ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];
			TestPanel.Children.Add (c);

			Enqueue ( delegate {
				Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState (), "#1");
				sb.Begin ();
			});

			EnqueueConditional (delegate { return sb.GetCurrentTime ().TotalMilliseconds > 300; }, TimeSpan.FromMilliseconds (600));
			Enqueue (delegate {
				// Animation1: 300/1000, Animation2: 300/2000
				Assert.IsLess (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity1");
				Assert.AreEqual (ClockState.Active, sb.GetCurrentState (), "#2");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [0]).GetCurrentState (), "#3");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#4");
			});

			EnqueueConditional (delegate { return sb.GetCurrentTime ().TotalMilliseconds > 1250; }, TimeSpan.FromMilliseconds (1200));
			Enqueue (delegate {
				// Animation1: 1250/1000, Animation2: 1250/2000
				Assert.IsGreater (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity2");
				Assert.AreEqual (ClockState.Active, sb.GetCurrentState (), "#5");
				Assert.AreEqual (ClockState.Filling, ((Storyboard) sb.Children [0]).GetCurrentState (), "#6");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#7");
			});

			EnqueueConditional (delegate { return sb.GetCurrentTime ().TotalMilliseconds > 1350; }, TimeSpan.FromMilliseconds (300));
			Enqueue (delegate {
				// Animation1: 1350/1000, Animation2: 1350/2000
				Assert.IsGreater (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity3");
				Assert.AreEqual (ClockState.Active, sb.GetCurrentState (), "#8");
				Assert.AreEqual (ClockState.Filling, ((Storyboard) sb.Children [0]).GetCurrentState (), "#9");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#10");
				sb.Stop ();
			});

			Enqueue (delegate {
				Assert.IsGreater (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity4");
				Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState (), "#11");
				Assert.AreEqual (ClockState.Filling, ((Storyboard) sb.Children [0]).GetCurrentState (), "#12");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#13");
			});

			Enqueue (delegate {
				// Animation1: 1450/1000, Animation2: 1450/2000
				Assert.IsGreater (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity5");
				Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState (), "#14");
				Assert.AreEqual (ClockState.Filling, ((Storyboard) sb.Children [0]).GetCurrentState (), "#15");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#16");
			});

			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		[MoonlightBug]
		public void CurrentState2 ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];
			TestPanel.Children.Add (c);

			Enqueue (delegate {
				Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState (), "#1");
				sb.Begin ();
			});

			EnqueueConditional (delegate { return sb.GetCurrentTime ().TotalMilliseconds > 300; });
			Enqueue (delegate {
				// Animation1: 300/1000, Animation2: 300/2000
				Assert.IsLess (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity1");
				Assert.AreEqual (ClockState.Active, sb.GetCurrentState (), "#2");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [0]).GetCurrentState (), "#3");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#4");
			});

			EnqueueConditional (delegate { return sb.GetCurrentTime ().TotalMilliseconds > 1250; });
			Enqueue (delegate {
				// Animation1: 1250/1000, Animation2: 1250/2000
				Assert.IsGreater (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity2");
				Assert.AreEqual (ClockState.Active, sb.GetCurrentState (), "#5");
				Assert.AreEqual (ClockState.Filling, ((Storyboard) sb.Children [0]).GetCurrentState (), "#6");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#7");
			});

			Enqueue (delegate {
				c.Children.Clear ();
				c.Resources.Clear ();
				TestPanel.Children.Clear ();
				TestPanel.Resources.Clear ();
			});

			EnqueueConditional (delegate { return sb.GetCurrentTime ().TotalMilliseconds > 1350; });
			Enqueue (delegate {
				// Animation1: 1350/1000, Animation2: 1350/2000
				Assert.IsGreater (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity3");
				Assert.AreEqual (ClockState.Active, sb.GetCurrentState (), "#8");
				Assert.AreEqual (ClockState.Filling, ((Storyboard) sb.Children [0]).GetCurrentState (), "#9");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#10");
				sb.Stop ();
			});

			Enqueue (delegate {
				Assert.IsGreater (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity4");
				Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState (), "#11");
				Assert.AreEqual (ClockState.Filling, ((Storyboard) sb.Children [0]).GetCurrentState (), "#12");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#13");
			});

			Enqueue (delegate {
				// Animation1: 1450/1000, Animation2: 1450/2000
				Assert.IsGreater (1, sb.GetCurrentTime ().TotalSeconds, "#Sanity5");
				Assert.AreEqual (ClockState.Stopped, sb.GetCurrentState (), "#14");
				Assert.AreEqual (ClockState.Filling, ((Storyboard) sb.Children [0]).GetCurrentState (), "#15");
				Assert.AreEqual (ClockState.Active, ((Storyboard) sb.Children [1]).GetCurrentState (), "#16");
			});

			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		[MoonlightBug]
		public void CompleteEvent ()
		{
			List<ManualResetEvent> handles = new List<ManualResetEvent> ();
			Canvas c = new Canvas ();

			Storyboard sb = new Storyboard { Duration = new Duration (TimeSpan.FromMilliseconds (500)) };
			ManualResetEvent handle = new ManualResetEvent (false);
			handles.Add (handle);
			bool fillingWhenComplete = false;
			bool childrenNotSignalled = true;
			sb.Completed += delegate {
				fillingWhenComplete = ClockState.Filling == sb.GetCurrentState ();
				//Assert.AreEqual (sb.Duration.TimeSpan, sb.GetCurrentTime (), "#2");
				for (int i = 1; i < handles.Count; i++)
					if (handles [i].WaitOne (0))
						childrenNotSignalled = false;
				handle.Set ();
			};
			c.Resources.Add ("Storyboard", sb);

			for (int i = 0; i < 5; i++) {
				DoubleAnimation anim = new DoubleAnimation { From = 10, To = 50 };
				Storyboard child = new Storyboard { Duration = new Duration (TimeSpan.FromMilliseconds (500)) };

				ManualResetEvent h = new ManualResetEvent (false);
				handles.Add (h);
				child.Completed += delegate {
					int count = i;
					h.Set ();
					Assert.AreEqual (ClockState.Filling, child.GetCurrentState (), "#4." + count);
				};

				child.Children.Add (anim);
				sb.Children.Add (child);
				Rectangle r = new Rectangle ();
				Storyboard.SetTarget (child, r);
				Storyboard.SetTargetProperty (child, new PropertyPath ("Width"));
				c.Children.Add (r);
			}

			TestPanel.Children.Add (c);
			for (int j = 0; j < 3; j++) {
				int temp = j;
				Enqueue (() => handles.ForEach (h => h.Reset ()));
				Enqueue (() => sb.Begin ());
				Enqueue (() => {
					Assert.AreEqual ( ClockState.Active, sb.GetCurrentState (), "#1");
				});
				EnqueueConditional (() => handle.WaitOne (0));
				Enqueue (() => Assert.IsTrue (fillingWhenComplete, "Filling when complete"));
				Enqueue (() => Assert.IsTrue (childrenNotSignalled, "Children not signalled"));
				Enqueue (() => {
					for (int i = 0; i < handles.Count; i++)
						Assert.IsTrue (handles [i].WaitOne (10));
				});
				Enqueue (() => Assert.AreEqual (ClockState.Filling, sb.GetCurrentState (), "Still filling"));
			}
			Enqueue (() => TestPanel.Children.Clear ());
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		[MoonlightBug]
		public void TargetSameProperty ()
		{
			Canvas c = new Canvas ();
			Rectangle r = new Rectangle ();
			c.Children.Add (r);

			Storyboard sb = new Storyboard { Duration = new Duration (TimeSpan.FromMilliseconds (500)) };
			c.Resources.Add ("Storyboard", sb);

			for (int i = 0; i < 5; i++) {
				DoubleAnimation anim = new DoubleAnimation { From = 10, To = 50 };
				Storyboard child = new Storyboard { Duration = new Duration (TimeSpan.FromMilliseconds (500)) };

				child.Children.Add (anim);
				sb.Children.Add (child);
				Storyboard.SetTarget (child, r);
				Storyboard.SetTargetProperty (child, new PropertyPath ("Width"));
			}

			CreateAsyncTest (c, () => Assert.Throws<InvalidOperationException>(() => sb.Begin ()));
		}
		
		[TestMethod]
		[Asynchronous]
		public void StartChildStoryboard ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];
			Storyboard child = (Storyboard) sb.Children [1];
			TestPanel.Children.Add (c);

			Enqueue (() => {
				Assert.Throws<InvalidOperationException> (() => child.Begin ());
			});
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		public void StopChildStoryboard ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard sb = (Storyboard) c.Resources ["Storyboard"];
			Storyboard child = (Storyboard) sb.Children [1];
			TestPanel.Children.Add (c);
			((Rectangle) c.Children [0]).Width = 0;
			int start = 0;

			Enqueue (() => { sb.Begin (); start = Environment.TickCount; });
			EnqueueConditional (() => Environment.TickCount - start > 100, "#0"); 
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => child.Stop (), "#1"));
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => child.Pause (), "#2"));
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => child.Seek(TimeSpan.FromMilliseconds(10)), "#3"));
			Enqueue (() => Assert.IsGreater (15, ((Rectangle) c.Children [0]).Width, "#5"));
			Enqueue (() => sb.Stop ());
			Enqueue (() => Assert.AreEqual (0, ((Rectangle) c.Children [0]).Width, "#6"));
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => child.Resume (), "#4"));
			EnqueueTestComplete ();
		}

		
		[TestMethod]
		[Asynchronous]
		public void ComplexTarget ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<DoubleAnimation Duration=""0:0:0.05"" Storyboard.TargetName=""target"" Storyboard.TargetProperty=""(UIElement.RenderTransform).Children[0].(RotateTransform.Angle)"" To=""50"" />
	<ColorAnimation Duration=""0:0:0.05"" Storyboard.TargetName=""target"" Storyboard.TargetProperty=""(Control.Background).(SolidColorBrush.Color)"" To=""Blue"" />
</Storyboard>");
			sb.Completed += delegate { complete = true; };
			Button b = new Button { Name="target" };
			b.RenderTransform = new TransformGroup ();
			((TransformGroup) b.RenderTransform).Children.Add (new RotateTransform ());
			b.Background = new SolidColorBrush (Colors.Black);
			Storyboard.SetTarget (sb, b);
			Enqueue (() => { TestPanel.Children.Add (b); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (Colors.Blue.ToString (), ((SolidColorBrush) b.Background).Color.ToString (), "#1"));
			Enqueue (() => Assert.AreEqual (50, (double) ((TransformGroup) b.RenderTransform).Children [0].GetValue (RotateTransform.AngleProperty), "#2"));
			Enqueue (()=> {TestPanel.Children.Clear(); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		public void ComplexTarget2 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<ColorAnimation Duration=""0:0:0.05"" Storyboard.TargetProperty=""(Shape.Fill).(GradientBrush.GradientStops)[0].(GradientStop.Color)"" To=""Red"" />
</Storyboard>");
			sb.Completed += delegate { complete = true; };

			Rectangle r = new Rectangle ();
			GradientBrush brush = new LinearGradientBrush ();
			brush.GradientStops.Add (new GradientStop { Color = Colors.Blue });
			r.Fill = brush;
			Storyboard.SetTarget (sb, r);
			Storyboard.SetTarget (sb.Children [0], r);

			Enqueue (() => { TestPanel.Children.Add (r); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (Colors.Red.ToString (), ((GradientBrush) r.Fill).GradientStops [0].GetValue (GradientStop.ColorProperty).ToString (), "#1"));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void ComplexTarget3 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<ColorAnimation Duration=""0:0:0.05"" Storyboard.TargetProperty=""Content.(Shape.Fill).(GradientBrush.GradientStops)[0].(GradientStop.Color)"" To=""Red"" />
</Storyboard>");
			sb.Completed += delegate { complete = true; };

			ContentControl c = new ContentControl();

			Rectangle r = new Rectangle ();
			GradientBrush brush = new LinearGradientBrush ();
			brush.GradientStops.Add (new GradientStop { Color = Colors.Blue });
			r.Fill = brush;
			c.Content = r;

			Storyboard.SetTarget (sb, c);
			Storyboard.SetTarget (sb.Children [0], c);

			Enqueue (() => { TestPanel.Children.Add (c); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => Assert.Throws<InvalidOperationException> (() => sb.Begin ()));
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		public void ComplexTarget4 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<ColorAnimation Duration=""0:0:0.05"" Storyboard.TargetProperty=""(Content).(Shape.Fill).(GradientBrush.GradientStops)[0].(GradientStop.Color)"" To=""Red"" />
</Storyboard>");
			sb.Completed += delegate { complete = true; };

			ContentControl c = new ContentControl ();

			Rectangle r = new Rectangle ();
			GradientBrush brush = new LinearGradientBrush ();
			brush.GradientStops.Add (new GradientStop { Color = Colors.Blue });
			r.Fill = brush;
			c.Content = r;

			Storyboard.SetTarget (sb, c);
			Storyboard.SetTarget (sb.Children [0], c);

			Enqueue (() => { TestPanel.Children.Add (c); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (Colors.Red.ToString (), ((GradientBrush) r.Fill).GradientStops [0].GetValue (GradientStop.ColorProperty).ToString (), "#1"));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		public void ComplexTarget5 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<ColorAnimation Duration=""0:0:0.05"" Storyboard.TargetProperty=""(ContentControl.Content).(Shape.Fill).(GradientBrush.GradientStops)[0].(GradientStop.Color)"" To=""Red"" />
</Storyboard>");
			sb.Completed += delegate { complete = true; };

			ContentControl c = new ContentControl ();

			Rectangle r = new Rectangle ();
			GradientBrush brush = new LinearGradientBrush ();
			brush.GradientStops.Add (new GradientStop { Color = Colors.Blue });
			r.Fill = brush;
			c.Content = r;

			Storyboard.SetTarget (sb, c);
			Storyboard.SetTarget (sb.Children [0], c);

			Enqueue (() => {TestPanel.Children.Add (c); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (Colors.Red.ToString (), ((GradientBrush) r.Fill).GradientStops [0].GetValue (GradientStop.ColorProperty).ToString (), "#1"));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void ComplexTarget6 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<ColorAnimation Duration=""0:0:0.05"" Storyboard.TargetName=""target"" Storyboard.TargetProperty=""(Control.Background).Color"" To=""Blue"" />
</Storyboard>");
			
			sb.Completed += delegate { complete = true; };
			Button b = new Button { Name = "target" };
			b.RenderTransform = new TransformGroup ();
			((TransformGroup) b.RenderTransform).Children.Add (new RotateTransform ());
			b.Background = new SolidColorBrush (Colors.Black);
			Storyboard.SetTarget (sb, b);
			Enqueue (() => { TestPanel.Children.Add (b); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (Colors.Blue.ToString (), ((SolidColorBrush) b.Background).Color.ToString (), "#1"));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}
		

		[TestMethod]
		[Asynchronous]
		public void ComplexTarget7 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<ColorAnimation Duration=""0:0:0.05"" Storyboard.TargetName=""target"" Storyboard.TargetProperty=""(Control.Background).(Color)"" To=""Blue"" />
</Storyboard>");

			sb.Completed += delegate { complete = true; };
			Button b = new Button { Name = "target" };
			b.RenderTransform = new TransformGroup ();
			((TransformGroup) b.RenderTransform).Children.Add (new RotateTransform ());
			b.Background = new SolidColorBrush (Colors.Black);
			Storyboard.SetTarget (sb, b);
			Enqueue (() => { TestPanel.Children.Add (b); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (Colors.Blue.ToString (), ((SolidColorBrush) b.Background).Color.ToString (), "#1"));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void ComplexTarget8 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<DoubleAnimation Duration=""0:0:0.05"" Storyboard.TargetProperty=""Canvas.Left"" To=""50"" />
</Storyboard>");
			Button b = new Button { Name="target" };
			Storyboard.SetTarget (sb, b);
			Enqueue (() => Assert.Throws<InvalidOperationException>(() => sb.Begin()));
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void ComplexTarget9 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<DoubleAnimation Duration=""0:0:0.05"" Storyboard.TargetProperty=""(Canvas.Left)"" From=""5"" To=""50"" />
</Storyboard>");
			sb.Completed += delegate { complete = true; };
			Rectangle b = new Rectangle ();
			Storyboard.SetTarget (sb, b);
			Enqueue (() => { TestPanel.Children.Add (b); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (50, Canvas.GetLeft (b), "#1"));
			EnqueueTestComplete ();
		}

		[TestMethod]
		[Asynchronous]
		public void ComplexTarget10 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<DoubleAnimation Duration=""0:0:0.05"" Storyboard.TargetName=""target"" Storyboard.TargetProperty=""Rectangle.RadiusX"" To=""50"" />
</Storyboard>");
			sb.Completed += delegate { complete = true; };
			Rectangle r = new Rectangle ();
			Storyboard.SetTarget (sb, r);
			Enqueue (() => { TestPanel.Children.Add (r); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (50, r.RadiusX));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		public void ComplexTarget11 ()
		{
			bool complete = false;
			Storyboard sb = (Storyboard) XamlReader.Load (
@"<Storyboard xmlns=""http://schemas.microsoft.com/winfx/2006/xaml/presentation""
              xmlns:x=""http://schemas.microsoft.com/winfx/2006/xaml"">
	<DoubleAnimation Duration=""0:0:0.05"" Storyboard.TargetName=""target"" Storyboard.TargetProperty=""(Rectangle.RadiusX)"" To=""50"" />
</Storyboard>");
			sb.Completed += delegate { complete = true; };
			Rectangle r = new Rectangle ();
			Storyboard.SetTarget (sb, r);
			Enqueue (() => { TestPanel.Children.Add (r); TestPanel.Resources.Add ("a", sb); });
			Enqueue (() => sb.Begin ());
			EnqueueConditional (() => complete);
			Enqueue (() => Assert.AreEqual (50, r.RadiusX));
			Enqueue (() => { TestPanel.Children.Clear (); TestPanel.Resources.Clear (); });
			EnqueueTestComplete ();
		}
		
		[TestMethod]
		[Asynchronous]
		[MoonlightBug]
		public void CurrentTime ()
		{
			Canvas c = CreateStoryboard ();
			Storyboard storyboard = (Storyboard) c.Resources ["Storyboard"];
			int start = Environment.TickCount;

			Enqueue (() => TestPanel.Children.Add (c));
			Enqueue (() => storyboard.Begin ());
			EnqueueConditional (()=> Environment.TickCount - start > 300);
			Enqueue (() => {
				double ms = storyboard.GetCurrentTime ().TotalMilliseconds;
				Assert.IsLess(350, ms, "Less than 350ms");
				Assert.AreEqual (((Storyboard) storyboard.Children [0]).GetCurrentTime ().TotalMilliseconds, ms, "#2");
				Assert.AreEqual (((Storyboard) storyboard.Children [1]).GetCurrentTime ().TotalMilliseconds, ms, "#3");
				double width = ((Rectangle) c.Children [0]).Width;
				Assert.IsBetween (20, 50, width, "#width1");
				Assert.IsLess (0.001, ((Rectangle)c.Children[1]).Width - width, "#width2");
			});
			EnqueueConditional (() => Environment.TickCount - start > 1100);
			Enqueue (() => {
				double ms = storyboard.GetCurrentTime ().TotalMilliseconds;
				Assert.IsGreater (1000, ms, "More than 1000ms");
				Assert.AreEqual (((Storyboard) storyboard.Children [0]).GetCurrentTime ().TotalMilliseconds, 1000, "#4");
				Assert.AreEqual (((Storyboard) storyboard.Children [1]).GetCurrentTime ().TotalMilliseconds, ms, "#5");
				double width = ((Rectangle) c.Children [0]).Width;
				Assert.AreEqual (100, width, "#width2");
				Assert.IsLess (0.001, ((Rectangle)c.Children[1]).Width - width, "#width3");
			});
			Enqueue (() => TestPanel.Children.Clear ());
			EnqueueTestComplete ();
		}

		private Canvas CreateStoryboard ()
		{
			Canvas c = new Canvas ();

			Storyboard sb = new Storyboard { RepeatBehavior = new RepeatBehavior (2) };

			Rectangle r = new Rectangle { Name = "A" };
			Storyboard child = new Storyboard ();
			DoubleAnimation animation = new DoubleAnimation { Duration = new Duration (TimeSpan.FromSeconds (1)), From = 10, To = 100 };
			Storyboard.SetTargetName (animation, "A");
			Storyboard.SetTargetProperty (animation, new PropertyPath ("Width"));

			child.Children.Add (animation);
			sb.Children.Add (child);
			c.Children.Add (r);

			r = new Rectangle { Name = "B" };
			child = new Storyboard ();
			animation = new DoubleAnimation { Duration = new Duration (TimeSpan.FromSeconds (2)), From = 10, To = 100 };
			Storyboard.SetTargetName (animation, "B");
			Storyboard.SetTargetProperty (animation, new PropertyPath (FrameworkElement.WidthProperty));

			child.Children.Add (animation);
			sb.Children.Add (child);
			c.Children.Add (r);

			c.Resources.Add ("Storyboard", sb);
			return c;
		}
	}
}
