#!/usr/bin/env python3
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import threading
import queue
import time
import serial
import serial.tools.list_ports as list_ports

# ---- Settings ----
BAUD = 9600
MAX_LASER_FREQUENCY = 200.0  # Hz

class SerialReader(threading.Thread):
    def __init__(self, ser, outq, stop_event):
        super().__init__(daemon=True)
        self.ser = ser
        self.outq = outq
        self.stop_event = stop_event

    def run(self):
        time.sleep(0.2)
        while not self.stop_event.is_set():
            try:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode(errors="replace").strip()
                    if line:
                        self.outq.put(line)
                else:
                    time.sleep(0.02)
            except serial.SerialException as e:
                self.outq.put(f"[ERROR] Serial exception: {e}")
                break

class PLDController(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("PLD Controller - All Features")
        self.geometry("800x600")
        
        # Status variables
        self.teach_done = False
        self.manual_mode = False
        self.laser_power_enabled = True
        self.experiment_running = False
        self.experiment_stop = threading.Event()
        self.motion_done = threading.Event()
        self.laser_done = threading.Event()
        
        # Serial
        self.ser = None
        self.reader_thread = None
        self.reader_stop = threading.Event()
        self.outq = queue.Queue()

        self.saved_positions_cache = {1: 0, 2: 267, 3: 533, 4: 800, 5: 1067, 6: 1333}  # Default Positionen
        
        self._build_ui()
        self.refresh_ports()
        self.after(20, self.drain_queue)

    def _build_ui(self):
        # Main container with scrollbar
        main_frame = ttk.Frame(self)
        main_frame.pack(fill="both", expand=True, padx=10, pady=10)
        
        # === SERIAL CONNECTION ===
        conn_frame = ttk.LabelFrame(main_frame, text="Serial Connection")
        conn_frame.pack(fill="x", pady=(0, 10))
        
        ttk.Label(conn_frame, text="Port:").grid(row=0, column=0, padx=5, pady=5)
        self.port_cmb = ttk.Combobox(conn_frame, width=25, state="readonly")
        self.port_cmb.grid(row=0, column=1, padx=5, pady=5)
        
        ttk.Button(conn_frame, text="Refresh", command=self.refresh_ports).grid(row=0, column=2, padx=5, pady=5)
        self.connect_btn = ttk.Button(conn_frame, text="Connect", command=self.toggle_connect)
        self.connect_btn.grid(row=0, column=3, padx=5, pady=5)
        
        # === STATUS ===
        status_frame = ttk.LabelFrame(main_frame, text="System Status")
        status_frame.pack(fill="x", pady=(0, 10))
        
        self.status_var = tk.StringVar(value="Disconnected")
        ttk.Label(status_frame, textvariable=self.status_var).pack(side="left", padx=5, pady=5)
        
        ttk.Button(status_frame, text="TEACH", command=lambda: self.send("CMD:TEACH")).pack(side="left", padx=5, pady=5)
        ttk.Button(status_frame, text="RESET", command=lambda: self.send("CMD:RESET")).pack(side="left", padx=5, pady=5)
        ttk.Button(status_frame, text="Get Position", command=lambda: self.send("CMD:POS")).pack(side="left", padx=5, pady=5)
        ttk.Button(status_frame, text="Manual Mode", command=self.enable_manual_mode).pack(side="left", padx=5, pady=5)
        ttk.Button(status_frame, text="Auto Mode", command=self.enable_auto_mode).pack(side="left", padx=5, pady=5)
        ttk.Button(status_frame, text="Status Check", command=self.safety_check).pack(side="left", padx=5, pady=5)

        # === POSITION CONTROL ===
        pos_frame = ttk.LabelFrame(main_frame, text="Position Control")
        pos_frame.pack(fill="x", pady=(0, 10))
        
        # Saved positions
        ttk.Label(pos_frame, text="Saved Positions:").grid(row=0, column=0, padx=5, pady=5)
        for i in range(6):
            btn = ttk.Button(pos_frame, text=f"Pos {i+1}", 
                           command=lambda i=i: self.send(f"CMD:LOAD:{i+1}"))
            btn.grid(row=0, column=i+1, padx=2, pady=5)
        
        # Goto position
        ttk.Label(pos_frame, text="Goto:").grid(row=1, column=0, padx=5, pady=5)
        self.goto_var = tk.IntVar(value=0)
        ttk.Entry(pos_frame, width=8, textvariable=self.goto_var).grid(row=1, column=1, padx=2, pady=5)
        ttk.Button(pos_frame, text="GO", command=self.goto_position).grid(row=1, column=2, padx=2, pady=5)
        
        ttk.Button(pos_frame, text="Save Current", command=self.save_position).grid(row=1, column=4, padx=10, pady=5)

        # === MOTOR SETTINGS ===
        motor_frame = ttk.LabelFrame(main_frame, text="Motor Settings")
        motor_frame.pack(fill="x", pady=(0, 10))

        # Max Speed
        ttk.Label(motor_frame, text="Max Speed:").grid(row=0, column=0, padx=5, pady=5)
        self.maxspeed_var = tk.DoubleVar(value=500.0)
        ttk.Entry(motor_frame, width=8, textvariable=self.maxspeed_var).grid(row=0, column=1, padx=2, pady=5)
        ttk.Button(motor_frame, text="Set", 
                command=lambda: self.send(f"CMD:SETMAXSPEED:{self.maxspeed_var.get()}")).grid(row=0, column=2, padx=2, pady=5)

        # Acceleration
        ttk.Label(motor_frame, text="Acceleration:").grid(row=0, column=3, padx=5, pady=5)
        self.accel_var = tk.DoubleVar(value=500.0)
        ttk.Entry(motor_frame, width=8, textvariable=self.accel_var).grid(row=0, column=4, padx=2, pady=5)
        ttk.Button(motor_frame, text="Set", 
                command=lambda: self.send(f"CMD:SETACCEL:{self.accel_var.get()}")).grid(row=0, column=5, padx=2, pady=5)

        # === LASER CONTROL ===
        laser_frame = ttk.LabelFrame(main_frame, text="Laser Control")
        laser_frame.pack(fill="x", pady=(0, 10))
        
        # Laser sequence
        ttk.Label(laser_frame, text="Pulses:").grid(row=0, column=0, padx=5, pady=5)
        self.pulses_var = tk.IntVar(value=10)
        ttk.Entry(laser_frame, width=8, textvariable=self.pulses_var).grid(row=0, column=1, padx=2, pady=5)
        
        ttk.Label(laser_frame, text="Freq (Hz):").grid(row=0, column=2, padx=5, pady=5)
        self.freq_var = tk.DoubleVar(value=1.0)
        ttk.Spinbox(laser_frame, from_=0.1, to=MAX_LASER_FREQUENCY, increment=0.1, width=8, textvariable=self.freq_var).grid(row=0, column=3, padx=2, pady=5)
        
        ttk.Button(laser_frame, text="Start Laser", command=self.start_laser).grid(row=0, column=4, padx=5, pady=5)
        ttk.Button(laser_frame, text="Stop Laser", command=self.stop_laser_and_experiment).grid(row=0, column=5, padx=2, pady=5)
        
        # Laser power
        ttk.Button(laser_frame, text="Kill Power", command=lambda: self.send("CMD:LASER_killp")).grid(row=1, column=4, padx=5, pady=5)
        ttk.Button(laser_frame, text="Restore Power", command=lambda: self.send("CMD:LASER_restorep")).grid(row=1, column=5, padx=2, pady=5)

        # === EXPERIMENT CONTROL ===
        exp_frame = ttk.LabelFrame(main_frame, text="Experiment Control")
        exp_frame.pack(fill="x", pady=(0, 10))
        
        # Experiment settings
        ttk.Label(exp_frame, text="Cycles:").grid(row=0, column=0, padx=5, pady=5)
        self.cycles_var = tk.IntVar(value=5)
        ttk.Entry(exp_frame, width=6, textvariable=self.cycles_var).grid(row=0, column=1, padx=2, pady=5)
        
        ttk.Label(exp_frame, text="Positions:").grid(row=0, column=2, padx=5, pady=5)
        self.positions_var = tk.IntVar(value=1)
        positions_cmb = ttk.Combobox(exp_frame, width=4, textvariable=self.positions_var, 
                                   values=[1, 2, 3, 4, 5, 6], state="readonly")
        positions_cmb.grid(row=0, column=3, padx=2, pady=5)
        positions_cmb.bind('<<ComboboxSelected>>', self.on_positions_changed)
        
        # Position configuration frame
        self.pos_config_frame = ttk.Frame(exp_frame)
        self.pos_config_frame.grid(row=1, column=0, columnspan=6, sticky="w", padx=5, pady=5)
        
        # Initialize position configuration
        self.position_slots = []
        self.position_shots = []
        self.position_frequencies = []
        self.create_position_config()
        
        # Control buttons
        self.start_exp_btn = ttk.Button(exp_frame, text="Start Experiment", 
                                      command=self.start_experiment)
        self.start_exp_btn.grid(row=2, column=0, columnspan=2, padx=5, pady=10)
        
        self.stop_exp_btn = ttk.Button(exp_frame, text="Stop Experiment", 
                                     command=self.stop_experiment, state="disabled")
        self.stop_exp_btn.grid(row=2, column=2, columnspan=2, padx=5, pady=10)
        
        self.progress_var = tk.StringVar(value="Ready")
        ttk.Label(exp_frame, textvariable=self.progress_var).grid(row=3, column=0, columnspan=6, pady=5)

        # === CUSTOM COMMAND ===
        cmd_frame = ttk.LabelFrame(main_frame, text="Custom Command")
        cmd_frame.pack(fill="x", pady=(0, 10))
        
        self.cmd_var = tk.StringVar()
        cmd_entry = ttk.Entry(cmd_frame, textvariable=self.cmd_var)
        cmd_entry.pack(side="left", fill="x", expand=True, padx=5, pady=5)
        cmd_entry.bind('<Return>', lambda e: self.send_command())
        
        ttk.Button(cmd_frame, text="Send", command=self.send_command).pack(side="left", padx=5, pady=5)

        # === COMMUNICATION LOG ===
        log_frame = ttk.LabelFrame(main_frame, text="Communication Log (Arduino Output)")
        log_frame.pack(fill="both", expand=True)
        
        # Text widget with scrollbars
        text_frame = ttk.Frame(log_frame)
        text_frame.pack(fill="both", expand=True, padx=5, pady=5)
        
        self.log_text = tk.Text(text_frame, wrap="word", height=12, state="disabled")
        v_scrollbar = ttk.Scrollbar(text_frame, orient="vertical", command=self.log_text.yview)
        h_scrollbar = ttk.Scrollbar(text_frame, orient="horizontal", command=self.log_text.xview)
        self.log_text.configure(yscrollcommand=v_scrollbar.set, xscrollcommand=h_scrollbar.set)
        
        self.log_text.pack(side="left", fill="both", expand=True)
        v_scrollbar.pack(side="right", fill="y")
        h_scrollbar.pack(side="bottom", fill="x")
        
        ttk.Button(log_frame, text="Clear Log", command=self.clear_log).pack(anchor="e", padx=5, pady=5)

    def create_position_config(self):
        """Create position configuration rows"""
        # Clear existing widgets
        for widget in self.pos_config_frame.winfo_children():
            widget.destroy()
        
        self.position_slots = []
        self.position_shots = []
        self.position_frequencies = []
        
        positions_count = self.positions_var.get()
        
        for i in range(positions_count):
            row = i
            
            # Position slot
            ttk.Label(self.pos_config_frame, text=f"Pos {i+1} Slot:").grid(row=row, column=0, padx=2, pady=2)
            slot_var = tk.IntVar(value=i+1)
            slot_cmb = ttk.Combobox(self.pos_config_frame, width=3, textvariable=slot_var, 
                                  values=[1, 2, 3, 4, 5, 6], state="readonly")
            slot_cmb.grid(row=row, column=1, padx=2, pady=2)
            self.position_slots.append(slot_var)
            
            # Shots
            ttk.Label(self.pos_config_frame, text="Shots:").grid(row=row, column=2, padx=2, pady=2)
            shots_var = tk.IntVar(value=3)
            ttk.Entry(self.pos_config_frame, width=6, textvariable=shots_var).grid(row=row, column=3, padx=2, pady=2)
            self.position_shots.append(shots_var)
            
            # Frequency
            ttk.Label(self.pos_config_frame, text="Freq (Hz):").grid(row=row, column=4, padx=2, pady=2)
            frequency_var = tk.DoubleVar(value=2.0)
            ttk.Entry(self.pos_config_frame, width=6, textvariable=frequency_var).grid(row=row, column=5, padx=2, pady=2)
            self.position_frequencies.append(frequency_var)

    def on_positions_changed(self, event):
        """Update position configuration when number of positions changes"""
        self.create_position_config()

    # === SERIAL METHODS ===
    def refresh_ports(self):
        ports = [f"{p.device} - {p.description}" for p in list_ports.comports()]
        self.port_cmb['values'] = ports if ports else ["No ports found"]
        if ports:
            self.port_cmb.current(0)

    def toggle_connect(self):
        if self.ser and self.ser.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_cmb.get().split(" - ")[0]
        if not port:
            messagebox.showwarning("No Port", "Please select a port")
            return
            
        try:
            self.ser = serial.Serial(port, BAUD, timeout=1)
            self.reader_stop.clear()
            self.reader_thread = SerialReader(self.ser, self.outq, self.reader_stop)
            self.reader_thread.start()
            
            self.connect_btn.config(text="Disconnect")
            self.status_var.set("Connected")
            self.log_line(f"✅ Connected to {port}")
        except serial.SerialException as e:
            messagebox.showerror("Connection Error", f"Failed to connect: {e}")

    def disconnect(self):
        if self.experiment_running:
            self.stop_experiment()
            
        if self.ser and self.ser.is_open:
            self.reader_stop.set()
            if self.reader_thread:
                self.reader_thread.join(timeout=1)
            self.ser.close()
            
        self.ser = None
        self.connect_btn.config(text="Connect")
        self.status_var.set("Disconnected")
        self.log_line("❌ Disconnected")

    def send(self, command):
        """Send command to Arduino"""
        # 🟢 AUSNAHMEN: Diese Befehle sind IMMER erlaubt (auch während Experiment)
        always_allowed_commands = (
            "CMD:LASER_stop",    # Sicherheit - Laser sofort stoppen
            "CMD:STATUS",        # Status abfragen
            "CMD:POS",           # Position abfragen
        )
        
        is_always_allowed = any(command.strip().startswith(cmd) for cmd in always_allowed_commands)
        
        # Während Experiment: Nur die "always_allowed" Befehle erlauben
        if self.experiment_running and not is_always_allowed:
            self.log_line("[WARN] Commands blocked during experiment - only STOP/STATUS allowed")
            return
            
        if not self.ser or not self.ser.is_open:
            self.log_line("[WARN] Not connected")
            return
            
        try:
            self.ser.write((command + "\n").encode())
            self.log_line(f"> {command}")
        except serial.SerialException as e:
            self.log_line(f"[ERROR] Send failed: {e}")

    def send_command(self):
        """Send custom command from entry"""
        cmd = self.cmd_var.get().strip()
        if cmd:
            self.send(cmd)
            self.cmd_var.set("")

    def send_status(self):
        self.send("CMD:STATUS")

    def safety_check(self):
        """Perform safety check before starting experiment"""
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Not Connected", "Please connect first")
            return
        self.send("CMD:STATUS")
        self.after(1000, self._evaluate_safety_status)

    def enable_manual_mode(self):
        if not self.ser or not self.ser.is_open:
            self.log_line("[WARN] Not connected")
            return
        self.send("CMD:MANUALLY")
        self.manual_mode = True
        self.teach_done = False  # Teach muss neu gemacht werden
        self.status_var.set("Connected - Manual Mode")

    def enable_auto_mode(self):
        if not self.ser or not self.ser.is_open:
            self.log_line("[WARN] Not connected")
            return
        self.send("CMD:AUTO")
        self.manual_mode = False
        self.status_var.set("Connected - Auto Mode")

    def _evaluate_safety_status(self):
        """Evaluate safety status from last log lines"""
        issues = []
        if not self.teach_done:
            issues.append("❌ Teach not completed")

        if self.manual_mode:
            issues.append("❌ System in Manual Mode")

        if not self.laser_power_enabled:
            issues.append("❌ Laser power disabled (killpower active)")

        if not issues:
            messagebox.showinfo("Safety Check", "✅ All systems ready:\n- Teach completed\n- Laser power enabled")
            self.log_line("[SAFETY CHECK] ✅ All systems ready")
        else:
            issue_text = "\n".join(issues)
            messagebox.showwarning("Safety Check Failed", 
                                 f"The following issues were found:\n\n{issue_text}\n\n"
                                 f"Please complete Teach and enable laser power before proceeding.")
            self.log_line(f"[SAFETY CHECK] ❌ Issues found: {', '.join(issues)}")

    # === POSITION METHODS ===
    def goto_position(self):
        pos = self.goto_var.get()
        if 0 <= pos <= 1599:
            self.send(f"CMD:GOTO:{pos}")
        else:
            messagebox.showwarning("Invalid", "Position must be 0-1599")

    def save_position(self):
        slot = simpledialog.askinteger("Save", "Slot (1-6):", minvalue=1, maxvalue=6)
        if slot:
            self.send(f"CMD:SAVE:{slot}")

    # === LASER METHODS ===
    def start_laser(self):
        pulses = self.pulses_var.get()
        freq = self.freq_var.get()
        if pulses <= 0 or freq <= 0 or freq > MAX_LASER_FREQUENCY:
            messagebox.showwarning("Invalid", "Pulses and Frequency must be > 0 and Frequency must be <= {MAX_LASER_FREQUENCY} Hz")
            return
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Not Connected", "Please connect first")
            return
        if not self.teach_done:
            proceed = messagebox.askyesno("Teach nicht abgeschlossen", "Tech ist NICHT abgeschlossen.\n\n Trotzdem Lasersequenz starten?")
            if not proceed:
                self.log_line("[ACTION] Lasersequenz abgebrochen - Teach nicht abgeschlossen")
                return
        if self.manual_mode:
            proceed = messagebox.askyesno("Manual Mode", "System ist im MANUELLEN Modus.\n\n Trotzdem Lasersequenz starten?")
            if not proceed:
                self.log_line("[ACTION] Lasersequenz abgebrochen - System im manuellen Modus")
                return

        self.send(f"CMD:LASER_p{pulses}f{freq}")

    def stop_laser_and_experiment(self):
        """Stop laser and experiment with one click"""
        if self.experiment_running:
            self.stop_experiment()
            self.log_line("[ACTION] Laser stopped and experiment cancelled")
        else:
            self.send("CMD:LASER_stop")
            self.log_line("[ACTION] Laser stopped")

    # === EXPERIMENT METHODS ===
    def start_experiment(self):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Not Connected", "Please connect first")
            return
        
        if self.manual_mode:
            messagebox.showwarning("Manual Mode", "Cannot start experiment in Manual Mode")
            return
            
        if not self.teach_done:
            messagebox.showwarning("Teach Required", "Teach not done."):
            return
        
        cycles = self.cycles_var.get()
        if cycles <= 0:
            messagebox.showwarning("Invalid", "Cycles must be > 0")
            return
            
        # Validate all positions
        for i in range(self.positions_var.get()):
            if self.position_shots[i].get() <= 0:
                messagebox.showwarning("Invalid", f"Shots for position {i+1} must be > 0")
                return
            if self.position_frequencies[i].get() <= 0 or self.position_frequencies[i].get() > MAX_LASER_FREQUENCY:
                messagebox.showwarning("Invalid", f"Frequency for position {i+1} must be between 0.1 and {MAX_LASER_FREQUENCY} Hz")
                return

        self.experiment_stop.clear()
        self.experiment_running = True
        self.start_exp_btn.config(state="disabled")
        self.stop_exp_btn.config(state="normal")
        self.progress_var.set("Experiment running...")
        
        # Start experiment in thread
        self.experiment_thread = threading.Thread(
            target=self._run_experiment,
            daemon=True
        )
        self.experiment_thread.start()
        
        self.log_line("[EXPERIMENT] Experiment started")

    def stop_experiment(self):
        if not self.experiment_running:
            return
        self.log_line("[EXPERIMENT] Stopping experiment...")
        self.experiment_stop.set()
        self.send("CMD:LASER_stop")

    
    def get_saved_position(self, slot):
        """Gib die gespeicherte Position für einen Slot zurück (aus Cache)"""
        # Verwende einfach den Cache - der wird automatisch durch parse_arduino_message aktualisiert
        if slot in self.saved_positions_cache:
            saved_pos = self.saved_positions_cache[slot]
            self.log_line(f"[INFO] Using cached position for slot {slot}: {saved_pos}")
            return saved_pos
        else:
            # Fallback falls Slot nicht im Cache ist
            fallback_positions = {1: 0, 2: 267, 3: 533, 4: 800, 5: 1067, 6: 1333}
            fallback_pos = fallback_positions.get(slot, 0)
            self.log_line(f"[WARN] Using fallback position for slot {slot}: {fallback_pos}")
            return fallback_pos

    def _run_experiment(self):
        try:
            cycles = self.cycles_var.get()
            positions_count = self.positions_var.get()

            for cycle in range(cycles):
                if self.experiment_stop.is_set():
                    break

                self.progress_var.set(f"Cycle {cycle+1}/{cycles}")
                self.log_line(f"[EXPERIMENT] Starting cycle {cycle+1}/{cycles}")

                for pos_idx in range(positions_count):
                    if self.experiment_stop.is_set():
                        break

                    slot  = self.position_slots[pos_idx].get()
                    shots = self.position_shots[pos_idx].get()
                    freq  = self.position_frequencies[pos_idx].get()

                    # --- 1) Move ---
                    self.motion_done.clear()
                    if not self._move_to_slot_and_wait(slot, pos_idx):
                        break

                    # --- 2) Laser ---
                    self.laser_done.clear()
                    if not self._fire_laser_and_wait(shots, freq, pos_idx):
                        break

            if not self.experiment_stop.is_set():
                self.log_line("[EXPERIMENT] Experiment completed successfully")
                self.progress_var.set("Experiment completed")

        except Exception as e:
            self.log_line(f"[EXPERIMENT ERROR] {e}")

        finally:
            self.after(0, self._experiment_finished)


    def _move_to_slot_and_wait(self, slot, pos_idx):
        try:
            self.motion_done.clear()
            move_cmd = f"CMD:LOAD:{slot}"
            self._experiment_send(move_cmd)
            self.log_line(f"[EXPERIMENT] Moving to slot {slot} (Pos {pos_idx+1})")

            if not self._wait_for_move_completion(slot, pos_idx):
                return False

            return True

        except Exception as e:
            self.log_line(f"[ERROR] Movement error: {e}")
            return False

        
    def _fire_laser_and_wait(self, shots, frequency, pos_idx):
        """fire laser and wait for OK:LASER_DONE"""
        self.laser_done.clear()
        cmd = f"CMD:LASER_p{shots}f{frequency}"

        self._experiment_send(cmd)
        self.log_line(f"[EXPERIMENT] Laser at pos {pos_idx+1}: {shots} pulses @ {frequency} Hz")

        # Worst-case Dauer (sicherer als estimate)
        real_freq = max(frequency*0.4, 0.5)
        estimated_time = shots / real_freq
        safe_timeout = estimated_time + 10
        safe_timeout = max(safe_timeout, 20)  # Mindestens 20s Timeout

        if not self.laser_done.wait(timeout=safe_timeout):
            self.log_line("[ERROR] Timeout waiting for laser")
            return False

        return True

        
    def _wait_for_move_completion(self, slot, pos_idx):
        self.motion_done.clear()
        if not self.motion_done.wait(timeout=30):
            return None  # Timeout
        return True

    def _wait_with_progress(self, wait_time, operation_name):
        """Warte mit Fortschritts-Updates und Stop-Check"""
        start_time = time.time()
        steps = max(1, int(wait_time))  # Maximal 1 Update pro Sekunde
        
        for i in range(steps + 1):
            if self.experiment_stop.is_set():
                return False
                
            elapsed = time.time() - start_time
            progress = min(100, int((elapsed / wait_time) * 100))
            
            if i < steps:  # Nicht beim letzten Step updaten
                self.progress_var.set(f"{operation_name}... {progress}%")
            
            time.sleep(1.0)  # 1 Sekunde warten
        
        return True

    def _experiment_send(self, command):
        """Send command during experiment (bypasses normal block)"""
        if not self.ser or not self.ser.is_open:
            self.log_line("[EXPERIMENT] Not connected.")
            return
        try:
            self.ser.write((command + "\n").encode())
            self.log_line(f"> {command}")
        except serial.SerialException as e:
            self.log_line(f"[ERROR] Send failed: {e}")
            self.experiment_stop.set()

    def _experiment_finished(self):
        """Called when experiment finishes"""
        self.experiment_running = False
        self.start_exp_btn.config(state="normal")
        self.stop_exp_btn.config(state="disabled")
        if not self.experiment_stop.is_set():
            self.progress_var.set("Experiment finished")
        else:
            self.progress_var.set("Experiment stopped")

    # === LOG METHODS ===
    def log_line(self, line: str):
        """Add line to log (thread-safe)"""
        self.log_text.config(state="normal")
        self.log_text.insert("end", line + "\n")
        self.log_text.see("end")
        self.log_text.config(state="disabled")

        # Statemachine Signale
        if "OK:TEACH" in line or "TEACH IST FERTIG" in line.upper():
            self.teach_done = True

        if "OK:GOTO" in line or "OK:LOAD" in line or "OK:MOVE_DONE" in line:
            self.motion_done.set()

        if "OK:LASER_DONE" in line:
            self.laser_done.set()
        
        # Update status from Arduino messages
        self.parse_arduino_message(line)

    def parse_arduino_message(self, line):
        """Parse messages from Arduino to update GUI status"""
        line_lower = line.lower()
        
        if "teach ist fertig" in line_lower or "teachdone: 1" in line_lower:
            self.teach_done = True
            self.status_var.set("Connected - Teach Done")
        elif "teach zurückgesetzt" in line_lower or "teachdone: 0" in line_lower:
            self.teach_done = False
            self.status_var.set("Connected - Teach Required")
  
        if ":" in line and len(line) < 10:  # Kurze Zeilen wie "1: 123"
            parts = line.split(":")
            if len(parts) == 2:  # Genau ein Doppelpunkt
                slot_str = parts[0].strip()
                pos_str = parts[1].strip()
                
                # Prüfe ob Slot eine Zahl zwischen 1-6 ist
                if slot_str.isdigit():
                    slot = int(slot_str)
                    if 1 <= slot <= 6:
                        # Prüfe ob Position eine gültige Zahl ist
                        if pos_str.replace('-', '').isdigit():
                            try:
                                position = int(pos_str)
                                if 0 <= position <= 1599:  # Gültiger Bereich
                                    self.saved_positions_cache[slot] = position
                            except ValueError:
                                pass
        if "relay: on" in line_lower:
            self.laser_power_enabled = True
        elif "relay: off" in line_lower:
            self.laser_power_enabled = False
        if "manueller modus aktiviert" in line_lower:
            self.manual_mode = True
            self.status_var.set("Connected - Manual Mode")
        if "automatischer modus aktiviert" in line_lower:
            self.manual_mode = False
            self.status_var.set("Connected - Auto Mode")

    def clear_log(self):
        self.log_text.config(state="normal")
        self.log_text.delete(1.0, "end")
        self.log_text.config(state="disabled")

    def drain_queue(self):
        """Process messages from serial reader"""
        try:
            while True:
                line = self.outq.get_nowait()
                self.log_line(line)
        except queue.Empty:
            pass
        self.after(20, self.drain_queue)

    def on_closing(self):
        """Clean up on window close"""
        if self.experiment_running:
            self.stop_experiment()
            time.sleep(0.5)
        self.disconnect()
        self.destroy()

if __name__ == "__main__":
    app = PLDController()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()