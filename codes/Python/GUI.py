import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import threading
import time
import pygame
import os


class DrumKitApp:
    def __init__(self, root):
        self.root = root
        self.root.title("404 NOT FOUND - Virtual Drum Kit")
        self.root.geometry("1400x800")
        self.root.configure(bg='#071426')
        
        pygame.mixer.init(frequency=44100, size=-16, channels=2, buffer=512)
        pygame.mixer.set_num_channels(16)  
        self.default_audio_paths = {
            'R_HAND': r'snare.wav',
            'L_HAND': r'hihat.wav',
            'R_FOOT': r'kick.wav'
        }
        
        self.sounds = {
            'L_HAND': None,
            'R_HAND': None,
            'R_FOOT': None
        }
        
        self.serial_port = None
        self.connected = False
        self.sheet = []
        self.step_state = []
        self.index = 0
        self.attempts = 0
        self.correct = 0
        self.wrong = 0
        self.matching = False
        self.read_thread = None
        self.running = False
        
        self.hit_history = []
        self.max_history = 20
        
        self.setup_ui()
        self.load_default_audio()
        
    def setup_ui(self):
        header = tk.Frame(self.root, bg='#071426')
        header.pack(fill='x', padx=20, pady=15)
        
        title_frame = tk.Frame(header, bg='#071426')
        title_frame.pack(side='left')
        
        tk.Label(title_frame, text="404 NOT FOUND", 
                font=('Poppins', 20, 'bold'), fg='#e8f0f6', bg='#071426').pack(anchor='w')
        tk.Label(title_frame, text="Virtual Drum Kit • Sheet Matcher • Live Tracking", 
                font=('Poppins', 11), fg='#8795a1', bg='#071426').pack(anchor='w')
        
        controls = tk.Frame(header, bg='#071426')
        controls.pack(side='right')
        
        tk.Label(controls, text="Serial Port:", fg='#8795a1', bg='#071426', 
                font=('Poppins', 10)).grid(row=0, column=0, padx=5)
        
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(controls, textvariable=self.port_var, width=15, state='readonly')
        self.refresh_ports()
        self.port_combo.grid(row=0, column=1, padx=5)
        
        self.refresh_btn = tk.Button(controls, text="↻", command=self.refresh_ports,
                                     bg='#16202b', fg='#00E5FF', font=('Poppins', 10, 'bold'),
                                     relief='flat', padx=8, pady=5, cursor='hand2')
        self.refresh_btn.grid(row=0, column=2, padx=2)
        
        self.connect_btn = tk.Button(controls, text="Connect", command=self.connect_serial,
                                     bg='#3ea0ff', fg='white', font=('Poppins', 10, 'bold'),
                                     relief='flat', padx=15, pady=5, cursor='hand2')
        self.connect_btn.grid(row=0, column=3, padx=5)
        
        self.disconnect_btn = tk.Button(controls, text="Disconnect", command=self.disconnect_serial,
                                       bg='#16202b', fg='#00E5FF', font=('Poppins', 10, 'bold'),
                                       relief='flat', padx=15, pady=5, cursor='hand2', state='disabled')
        self.disconnect_btn.grid(row=0, column=4, padx=5)
        
        main = tk.Frame(self.root, bg='#071426')
        main.pack(fill='both', expand=True, padx=20, pady=10)
        
        left = tk.Frame(main, bg='#0b2033', relief='raised', bd=1)
        left.pack(side='left', fill='both', expand=True, padx=(0, 10))
        
        audio_frame = tk.Frame(left, bg='#0f1a26', relief='raised', bd=1)
        audio_frame.pack(fill='x', padx=15, pady=15)
        
        tk.Label(audio_frame, text="🎵 Audio Files", fg='#e8f0f6', bg='#0f1a26', 
                font=('Poppins', 11, 'bold')).pack(anchor='w', padx=10, pady=10)
        
        audio_btns = tk.Frame(audio_frame, bg='#0f1a26')
        audio_btns.pack(fill='x', padx=10, pady=(0, 10))
        
        for i, (key, name) in enumerate([('R_HAND', 'Snare (RH)'), ('L_HAND', 'Hi-Hat (LH)'), ('R_FOOT', 'Kick (RF)')]):
            btn_frame = tk.Frame(audio_btns, bg='#0f1a26')
            btn_frame.grid(row=0, column=i, padx=8)
            
            tk.Button(btn_frame, text=f"Change {name}", 
                     command=lambda k=key: self.load_audio(k),
                     bg='#3ea0ff', fg='white', font=('Poppins', 8, 'bold'),
                     relief='flat', padx=8, pady=4, cursor='hand2').pack()
            
            status = tk.Label(btn_frame, text="Not loaded", fg='#FF6B6B', bg='#0f1a26', 
                            font=('Poppins', 7))
            status.pack()
            setattr(self, f'audio_status_{key}', status)
        
        drum_frame = tk.Frame(left, bg='#0b2033')
        drum_frame.pack(pady=15)
        
        self.drums = {}
        drum_config = [
            ('L_HAND', 'Left Hand', 'LH'),
            ('R_HAND', 'Right Hand', 'RH'),
            ('R_FOOT', 'Right Foot', 'RF')
        ]
        
        for i, (key, name, label) in enumerate(drum_config):
            drum = tk.Canvas(drum_frame, width=150, height=150, bg='#2b3b4d', 
                           highlightthickness=1, highlightbackground='#1a2a3d',
                           cursor='hand2')
            drum.grid(row=0, column=i, padx=15)
            drum.create_oval(10, 10, 140, 140, fill='#2b3b4d', outline='#3a4a5d', width=2)
            drum.create_text(25, 20, text=label, fill='#8795a1', font=('Poppins', 9, 'bold'))
            drum.create_text(75, 75, text=name, fill='white', font=('Poppins', 12, 'bold'))
            
            drum.bind('<Button-1>', lambda e, k=key: self.play_sound(k))
            
            self.drums[key] = drum
        
        sheet_frame = tk.Frame(left, bg='#0f1a26', relief='raised', bd=1)
        sheet_frame.pack(fill='both', expand=True, padx=15, pady=15)
        
        tk.Label(sheet_frame, text="📋 Sheet Music", fg='#e8f0f6', bg='#0f1a26', 
                font=('Poppins', 11, 'bold')).pack(anchor='w', padx=10, pady=10)
        
        self.sheet_input = tk.Text(sheet_frame, height=2, bg='#000000', fg='#e8f0f6',
                                   font=('Poppins', 11, 'bold'), relief='flat', padx=10, pady=8)
        self.sheet_input.pack(fill='x', padx=10, pady=5)
        
        sheet_controls = tk.Frame(sheet_frame, bg='#0f1a26')
        sheet_controls.pack(pady=8)
        
        tk.Button(sheet_controls, text="Load", command=self.load_from_input,
                 bg='#3ea0ff', fg='white', font=('Poppins', 9, 'bold'),
                 relief='flat', padx=15, pady=6, cursor='hand2').grid(row=0, column=0, padx=4)
        
        self.start_btn = tk.Button(sheet_controls, text="▶ Start", command=self.start_pause_toggle,
                                   bg='#26A69A', fg='white', font=('Poppins', 9, 'bold'),
                                   relief='flat', padx=15, pady=6, cursor='hand2')
        self.start_btn.grid(row=0, column=1, padx=4)
        
        tk.Button(sheet_controls, text="Reset", command=self.reset_sheet,
                 bg='#16202b', fg='#00E5FF', font=('Poppins', 9, 'bold'),
                 relief='flat', padx=15, pady=6, cursor='hand2').grid(row=0, column=2, padx=4)
        
        tk.Button(sheet_controls, text="Import .txt", command=self.import_file,
                 bg='#3ea0ff', fg='white', font=('Poppins', 9, 'bold'),
                 relief='flat', padx=15, pady=6, cursor='hand2').grid(row=0, column=3, padx=4)
        
        display_container = tk.Frame(sheet_frame, bg='#0f1a26')
        display_container.pack(fill='both', expand=True, padx=10, pady=10)
        
        sheet_canvas = tk.Canvas(display_container, bg='#000000', highlightthickness=0, height=100)
        h_scrollbar = tk.Scrollbar(display_container, orient='horizontal', command=sheet_canvas.xview)
        self.sheet_display = tk.Frame(sheet_canvas, bg='#000000')
        
        self.sheet_display.bind(
            '<Configure>',
            lambda e: sheet_canvas.configure(scrollregion=sheet_canvas.bbox('all'))
        )
        
        sheet_canvas.create_window((0, 0), window=self.sheet_display, anchor='nw')
        sheet_canvas.configure(xscrollcommand=h_scrollbar.set)
        
        sheet_canvas.pack(side='top', fill='both', expand=True)
        h_scrollbar.pack(side='bottom', fill='x')
        
        self.sheet_canvas = sheet_canvas
        
        right = tk.Frame(main, bg='#0b2033', width=450, relief='raised', bd=1)
        right.pack(side='right', fill='both')
        right.pack_propagate(False)
        
        stats_panel = tk.Frame(right, bg='#0f1a26', relief='raised', bd=1)
        stats_panel.pack(fill='x', padx=15, pady=15)
        
        tk.Label(stats_panel, text="📊 Performance Stats", fg='#e8f0f6', bg='#0f1a26', 
                font=('Poppins', 12, 'bold')).pack(anchor='w', padx=10, pady=10)
        
        stats_grid = tk.Frame(stats_panel, bg='#0f1a26')
        stats_grid.pack(fill='x', padx=10, pady=(0, 15))
        
        prog_card = tk.Frame(stats_grid, bg='#1a2a3d', relief='flat', bd=0)
        prog_card.grid(row=0, column=0, padx=5, pady=5, sticky='ew')
        tk.Label(prog_card, text="Progress", fg='#8795a1', bg='#1a2a3d', 
                font=('Poppins', 9)).pack(pady=(8, 2))
        self.progress_label = tk.Label(prog_card, text="0 / 0", fg='#e8f0f6', 
                                      bg='#1a2a3d', font=('Poppins', 16, 'bold'))
        self.progress_label.pack(pady=(0, 8))
        
        att_card = tk.Frame(stats_grid, bg='#1a2a3d', relief='flat', bd=0)
        att_card.grid(row=0, column=1, padx=5, pady=5, sticky='ew')
        tk.Label(att_card, text="Attempts", fg='#8795a1', bg='#1a2a3d', 
                font=('Poppins', 9)).pack(pady=(8, 2))
        self.attempts_label = tk.Label(att_card, text="0", fg='#00E5FF', 
                                      bg='#1a2a3d', font=('Poppins', 16, 'bold'))
        self.attempts_label.pack(pady=(0, 8))
        
        cor_card = tk.Frame(stats_grid, bg='#1a2a3d', relief='flat', bd=0)
        cor_card.grid(row=1, column=0, padx=5, pady=5, sticky='ew')
        tk.Label(cor_card, text="✅ Correct", fg='#8795a1', bg='#1a2a3d', 
                font=('Poppins', 9)).pack(pady=(8, 2))
        self.correct_label = tk.Label(cor_card, text="0", fg='#26A69A', 
                                     bg='#1a2a3d', font=('Poppins', 16, 'bold'))
        self.correct_label.pack(pady=(0, 8))
        
        wrong_card = tk.Frame(stats_grid, bg='#1a2a3d', relief='flat', bd=0)
        wrong_card.grid(row=1, column=1, padx=5, pady=5, sticky='ew')
        tk.Label(wrong_card, text="❌ Wrong", fg='#8795a1', bg='#1a2a3d', 
                font=('Poppins', 9)).pack(pady=(8, 2))
        self.wrong_label = tk.Label(wrong_card, text="0", fg='#FF6B6B', 
                                   bg='#1a2a3d', font=('Poppins', 16, 'bold'))
        self.wrong_label.pack(pady=(0, 8))
        
        stats_grid.columnconfigure(0, weight=1)
        stats_grid.columnconfigure(1, weight=1)
        
        acc_card = tk.Frame(stats_panel, bg='#1a2a3d', relief='flat', bd=0)
        acc_card.pack(fill='x', padx=10, pady=(5, 15))
        tk.Label(acc_card, text="🎯 Accuracy", fg='#8795a1', bg='#1a2a3d', 
                font=('Poppins', 11, 'bold')).pack(pady=(12, 5))
        self.accuracy_label = tk.Label(acc_card, text="0%", fg='#FFD700', 
                                      bg='#1a2a3d', font=('Poppins', 32, 'bold'))
        self.accuracy_label.pack(pady=(0, 12))
        
        live_feed_panel = tk.Frame(right, bg='#0f1a26', relief='raised', bd=1)
        live_feed_panel.pack(fill='both', expand=True, padx=15, pady=(0, 15))
        
        tk.Label(live_feed_panel, text="🎵 Live Hit Feed", fg='#e8f0f6', bg='#0f1a26', 
                font=('Poppins', 12, 'bold')).pack(anchor='w', padx=10, pady=10)
        
        feed_container = tk.Frame(live_feed_panel, bg='#0f1a26')
        feed_container.pack(fill='both', expand=True, padx=10, pady=(0, 10))
        
        feed_canvas = tk.Canvas(feed_container, bg='#000000', highlightthickness=0)
        scrollbar = tk.Scrollbar(feed_container, orient='vertical', command=feed_canvas.yview)
        self.live_feed_frame = tk.Frame(feed_canvas, bg='#000000')
        
        self.live_feed_frame.bind(
            '<Configure>',
            lambda e: feed_canvas.configure(scrollregion=feed_canvas.bbox('all'))
        )
        
        feed_canvas.create_window((0, 0), window=self.live_feed_frame, anchor='nw')
        feed_canvas.configure(yscrollcommand=scrollbar.set)
        
        feed_canvas.pack(side='left', fill='both', expand=True)
        scrollbar.pack(side='right', fill='y')
        
        self.feed_canvas = feed_canvas
    
    def load_default_audio(self):
        """Load default audio files on startup"""
        for drum_key, path in self.default_audio_paths.items():
            if os.path.exists(path):
                try:
                    self.sounds[drum_key] = pygame.mixer.Sound(path)
                    status_label = getattr(self, f'audio_status_{drum_key}')
                    status_label.config(text="✓ Default", fg='#26A69A')
                    print(f"✅ Loaded default audio for {drum_key}: {os.path.basename(path)}")
                except Exception as e:
                    print(f"❌ Failed to load default audio for {drum_key}: {e}")
            else:
                print(f"⚠️  Default audio file not found for {drum_key}: {path}")
    
    def load_audio(self, drum_key):
        """Load custom audio file for specific drum"""
        filename = filedialog.askopenfilename(
            title=f"Select WAV file for {drum_key}",
            filetypes=[("WAV files", "*.wav"), ("All files", "*.*")]
        )
        if filename:
            try:
                self.sounds[drum_key] = pygame.mixer.Sound(filename)
                status_label = getattr(self, f'audio_status_{drum_key}')
                status_label.config(text="✓ Custom", fg='#00E5FF')
                print(f"✅ Loaded custom audio for {drum_key}: {os.path.basename(filename)}")
            except Exception as e:
                messagebox.showerror("Audio Error", f"Failed to load audio: {str(e)}")
                print(f"❌ Failed to load audio for {drum_key}: {e}")
    
    def play_sound(self, drum_key):
        """Play sound for specific drum - allows overlapping/mixing"""
        if self.sounds[drum_key]:
            self.sounds[drum_key].play()
        else:
            print(f"⚠️  No audio loaded for {drum_key}")
    
    def add_to_live_feed(self, token, judgement):
        """Add hit to live feed display"""
        timestamp = time.strftime("%H:%M:%S")
        self.hit_history.append((token, judgement, timestamp))
        
        if len(self.hit_history) > self.max_history:
            self.hit_history.pop(0)
        
        self.update_live_feed()
    
    def update_live_feed(self):
        """Update the live feed display"""
        for widget in self.live_feed_frame.winfo_children():
            widget.destroy()
        
        for token, judgement, timestamp in reversed(self.hit_history):
            hit_frame = tk.Frame(self.live_feed_frame, bg='#000000')
            hit_frame.pack(fill='x', padx=5, pady=3)
            
            if judgement == 'correct':
                bg_color = '#26A69A'
                fg_color = '#000000'
                symbol = '✅'
            elif judgement == 'wrong':
                bg_color = '#FF6B6B'
                fg_color = '#FFFFFF'
                symbol = '❌'
            else:
                bg_color = '#7C83FF'
                fg_color = '#FFFFFF'
                symbol = '🔵'
            
            token_label = tk.Label(hit_frame, text=f"{symbol} {token}", 
                                  bg=bg_color, fg=fg_color,
                                  font=('Poppins', 10, 'bold'),
                                  padx=10, pady=5, relief='flat')
            token_label.pack(side='left', padx=(0, 10))
            
            time_label = tk.Label(hit_frame, text=timestamp, 
                                 fg='#8795a1', bg='#000000',
                                 font=('Poppins', 8))
            time_label.pack(side='right')
        
        self.feed_canvas.yview_moveto(0)
    
    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.current(0)
    
    def connect_serial(self):
        port = self.port_var.get()
        if not port:
            messagebox.showerror("Error", "Please select a serial port")
            return
        
        try:
            self.serial_port = serial.Serial(port, 9600, timeout=0.1)
            self.connected = True
            self.connect_btn.config(state='disabled')
            self.disconnect_btn.config(state='normal')
            
            print(f"\n✅ Connected to {port}")
            print("=" * 50)
            print("Listening for data... (1=RH, 2=LH, 3=RF)")
            print("=" * 50)
            
            self.running = True
            self.read_thread = threading.Thread(target=self.read_serial, daemon=True)
            self.read_thread.start()
            
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))
            print(f"❌ Connection Error: {e}")
    
    def disconnect_serial(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        self.connected = False
        self.connect_btn.config(state='normal')
        self.disconnect_btn.config(state='disabled')
        print("\n⭕ Disconnected from serial port\n")
    
    def read_serial(self):
        while self.running:
            try:
                if self.serial_port and self.serial_port.in_waiting:
                    line = self.serial_port.readline().decode('utf-8').strip()
                    if line:
                        print(f"📥 Received: '{line}'")
                        self.root.after(0, self.on_hit_received, line)
            except Exception as e:
                print(f"❌ Serial read error: {e}")
            time.sleep(0.01)
    
    def map_token_to_id(self, token):
        if not token:
            return None
        token = token.strip().upper()
        if token in ['LH', 'L_HAND', 'L-HAND', '2']:
            return 'L_HAND'
        if token in ['RH', 'R_HAND', 'R-HAND', '1']:
            return 'R_HAND'
        if token in ['RF', 'R_FOOT', 'R-FOOT', '3']:
            return 'R_FOOT'
        return None
    
    def get_drum_name(self, drum_id):
        names = {
            'L_HAND': 'Left Hand (Hi-Hat)',
            'R_HAND': 'Right Hand (Snare)',
            'R_FOOT': 'Right Foot (Kick)'
        }
        return names.get(drum_id, drum_id)
    
    def get_display_token(self, drum_id):
        """Get short display token"""
        mapping = {
            'L_HAND': 'LH',
            'R_HAND': 'RH',
            'R_FOOT': 'RF'
        }
        return mapping.get(drum_id, drum_id)
    
    def animate_pad(self, token, judgement=None):
        drum_id = self.map_token_to_id(token)
        if not drum_id or drum_id not in self.drums:
            print(f"⚠️  Unknown token: '{token}'")
            return
        
        self.play_sound(drum_id)
        
        display_token = self.get_display_token(drum_id)
        self.add_to_live_feed(display_token, judgement if judgement else 'neutral')
        
        drum = self.drums[drum_id]
        
        if judgement == 'correct':
            color = '#26A69A'
            status = '✅ CORRECT'
        elif judgement == 'wrong':
            color = '#FF6B6B'
            status = '❌ WRONG'
        else:
            color = '#7C83FF'
            status = '🔵 HIT'
        
        drum_name = self.get_drum_name(drum_id)
        print(f"   {status} → {drum_name}")
        
        drum.configure(bg=color)
        drum.itemconfig(1, fill=color)
        self.root.after(200, lambda: self.reset_drum_color(drum))
    
    def reset_drum_color(self, drum):
        drum.configure(bg='#2b3b4d')
        drum.itemconfig(1, fill='#2b3b4d')
    
    def on_hit_received(self, raw_token):
        token = self.map_token_to_id(raw_token)
        
        if not self.matching or self.index >= len(self.sheet):
            self.animate_pad(raw_token, None)
            return
        
        expected_token = self.sheet[self.index]
        self.attempts += 1
        
        if self.map_token_to_id(token) == self.map_token_to_id(expected_token):
            self.correct += 1
            self.step_state[self.index] = 'correct'
            self.animate_pad(raw_token, 'correct')
        else:
            self.wrong += 1
            self.step_state[self.index] = 'wrong'
            self.animate_pad(raw_token, 'wrong')
            print(f"   Expected: {expected_token}")
        
        self.index += 1
        
        if self.index >= len(self.sheet):
            self.matching = False
            self.start_btn.config(text="▶ Start", bg='#26A69A')
            accuracy = round((self.correct / self.attempts) * 100) if self.attempts > 0 else 0
            print(f"\n🎯 SHEET COMPLETE! Accuracy: {accuracy}%\n")
        
        self.render_sheet()
        self.update_progress()
        
        # Auto-scroll to current position
        if self.matching and self.index < len(self.sheet):
            self.sheet_canvas.xview_moveto((self.index / len(self.sheet)) * 0.8)
    
    def start_pause_toggle(self):
        if len(self.sheet) == 0:
            messagebox.showwarning("Warning", "Please load a sheet first")
            return
        
        self.matching = not self.matching
        
        if self.matching:
            self.start_btn.config(text="⏸ Pause", bg='#FF9500')
            print(f"\n▶️  MATCHING STARTED - Sheet: {' '.join(self.sheet)}\n")
        else:
            self.start_btn.config(text="▶ Start", bg='#26A69A')
            print(f"\n⏸️  MATCHING PAUSED\n")
        
        self.render_sheet()
    
    def reset_sheet(self):
        self.matching = False
        self.index = 0
        self.attempts = 0
        self.correct = 0
        self.wrong = 0
        self.step_state = ['pending'] * len(self.sheet)
        self.start_btn.config(text="▶ Start", bg='#26A69A')
        self.render_sheet()
        self.update_progress()
        print("\n🔄 Sheet reset\n")
    
    def load_from_input(self):
        text = self.sheet_input.get('1.0', tk.END)
        self.load_sheet_from_text(text)
    
    def import_file(self):
        filename = filedialog.askopenfilename(
            title="Select sheet file",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        if filename:
            with open(filename, 'r') as f:
                content = f.read()
            self.sheet_input.delete('1.0', tk.END)
            self.sheet_input.insert('1.0', content)
            self.load_sheet_from_text(content)
    
    def load_sheet_from_text(self, text):
        tokens = text.strip().upper().split()
        self.sheet = [self.normalize_token(t) for t in tokens if t]
        self.step_state = ['pending'] * len(self.sheet)
        self.index = 0
        self.attempts = 0
        self.correct = 0
        self.wrong = 0
        self.matching = False
        self.start_btn.config(text="▶ Start", bg='#26A69A')
        self.render_sheet()
        self.update_progress()
        print(f"\n📋 Loaded sheet ({len(self.sheet)} steps): {' '.join(self.sheet)}\n")
    
    def normalize_token(self, token):
        mapped = self.map_token_to_id(token)
        if mapped == 'L_HAND':
            return 'LH'
        elif mapped == 'R_HAND':
            return 'RH'
        elif mapped == 'R_FOOT':
            return 'RF'
        return token
    
    def render_sheet(self):
        """Render sheet with clear visual progress tracking"""
        for widget in self.sheet_display.winfo_children():
            widget.destroy()
        
        for i, token in enumerate(self.sheet):
            # Default colors
            bg_color = '#1a2a3d'
            fg_color = '#8795a1'
            border_color = '#2a3a4d'
            
            # Current position (yellow highlight)
            if i == self.index and self.matching:
                bg_color = '#FFD700'
                fg_color = '#000000'
                border_color = '#FFA500'
            # Correct (green)
            elif self.step_state[i] == 'correct':
                bg_color = '#26A69A'
                fg_color = '#FFFFFF'
                border_color = '#1E8E7E'
            # Wrong (red)
            elif self.step_state[i] == 'wrong':
                bg_color = '#FF6B6B'
                fg_color = '#FFFFFF'
                border_color = '#CC5555'
            # Already played (dimmed)
            elif i < self.index:
                bg_color = '#0a1520'
                fg_color = '#4a5a6a'
                border_color = '#1a2530'
            
            step_frame = tk.Frame(self.sheet_display, bg=border_color, padx=1, pady=1)
            step_frame.pack(side='left', padx=3, pady=3)
            
            step = tk.Label(step_frame, text=token, bg=bg_color, fg=fg_color,
                          font=('Poppins', 14, 'bold'), padx=12, pady=8, relief='flat')
            step.pack()
    
    def update_progress(self):
        """Update all progress stats"""
        current = min(self.index, len(self.sheet))
        total = len(self.sheet)
        
        self.progress_label.config(text=f"{current} / {total}")
        self.attempts_label.config(text=str(self.attempts))
        self.correct_label.config(text=str(self.correct))
        self.wrong_label.config(text=str(self.wrong))
        
        # Calculate and display accuracy
        if self.attempts > 0:
            accuracy = round((self.correct / self.attempts) * 100)
            self.accuracy_label.config(text=f"{accuracy}%")
            
            # Color code accuracy
            if accuracy >= 90:
                self.accuracy_label.config(fg='#26A69A')  # Green
            elif accuracy >= 70:
                self.accuracy_label.config(fg='#FFD700')  # Gold
            else:
                self.accuracy_label.config(fg='#FF6B6B')  # Red
        else:
            self.accuracy_label.config(text="0%", fg='#8795a1')


if __name__ == "__main__":
    root = tk.Tk()
    app = DrumKitApp(root)
    
    print("\n" + "="*50)
    print("🥁 404 NOT FOUND - Virtual Drum Kit")
    print("="*50)
    print("\nToken mapping:")
    print("  1 → Right Hand / Snare (RH)")
    print("  2 → Left Hand / Hi-Hat (LH)")
    print("  3 → Right Foot / Kick Drum (RF)")
    print("\n🎛️  Features:")
    print("  • 16-channel audio mixing")
    print("  • Real-time progress tracking")
    print("  • Live hit feed")
    print("  • Auto-scrolling sheet")
    print("\nDefault audio files loaded (if paths are correct)")
    print("="*50 + "\n")
    
    root.mainloop()
