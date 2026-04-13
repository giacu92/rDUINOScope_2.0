#!/usr/bin/env python3
"""
rDUINOScope 2.0 - Hex to Degrees Converter GUI
Convert hexadecimal Modbus registers to telescope pointing degrees (RA and DEC).
"""

import tkinter as tk
from tkinter import ttk
import re


class HexDegreesConverter:
    def __init__(self, root):
        self.root = root
        self.root.title("rDUINOScope 2.0 - Hex/Degrees Converter")
        self.root.geometry("600x600")
        self.root.resizable(False, False)
        
        # Configure style
        style = ttk.Style()
        style.theme_use('clam')
        
        self.create_widgets()
    
    def create_widgets(self):
        """Create GUI elements"""
        
        # Main frame
        main_frame = ttk.Frame(self.root, padding="15")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Title
        title = ttk.Label(main_frame, text="Hex ↔ Degrees Converter", 
                         font=("Arial", 14, "bold"))
        title.pack(pady=(0, 20))
        
        # Notebook (tabbed interface)
        notebook = ttk.Notebook(main_frame)
        notebook.pack(fill=tk.BOTH, expand=True)
        
        # Tab 1: Hex to Degrees
        self.create_hex_to_deg_tab(notebook)
        
        # Tab 2: Degrees to Hex
        self.create_deg_to_hex_tab(notebook)
    
    def create_hex_to_deg_tab(self, notebook):
        """Create Hex → Degrees tab"""
        
        frame = ttk.Frame(notebook, padding="15")
        notebook.add(frame, text="Hex → Degrees")
        
        # RA Section
        ra_frame = ttk.LabelFrame(frame, text="Right Ascension (RA)", padding="10")
        ra_frame.pack(fill=tk.X, pady=(0, 15))
        
        ttk.Label(ra_frame, text="HI Register (hex):").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.hex_ra_hi = ttk.Entry(ra_frame, width=15)
        self.hex_ra_hi.grid(row=0, column=1, sticky=tk.W, padx=5)
        
        ttk.Label(ra_frame, text="LO Register (hex):").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.hex_ra_lo = ttk.Entry(ra_frame, width=15)
        self.hex_ra_lo.grid(row=1, column=1, sticky=tk.W, padx=5)
        
        # DEC Section
        dec_frame = ttk.LabelFrame(frame, text="Declination (DEC)", padding="10")
        dec_frame.pack(fill=tk.X, pady=(0, 15))
        
        ttk.Label(dec_frame, text="HI Register (hex):").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.hex_dec_hi = ttk.Entry(dec_frame, width=15)
        self.hex_dec_hi.grid(row=0, column=1, sticky=tk.W, padx=5)
        
        ttk.Label(dec_frame, text="LO Register (hex):").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.hex_dec_lo = ttk.Entry(dec_frame, width=15)
        self.hex_dec_lo.grid(row=1, column=1, sticky=tk.W, padx=5)
        
        # Convert button
        btn_frame = ttk.Frame(frame)
        btn_frame.pack(fill=tk.X, pady=15)
        
        ttk.Button(btn_frame, text="Convert", command=self.convert_hex_to_deg,
                  width=20).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Clear", command=self.clear_hex_to_deg,
                  width=20).pack(side=tk.LEFT, padx=5)
        
        # Results frame
        results_frame = ttk.LabelFrame(frame, text="Results", padding="10")
        results_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(results_frame, text="RA Decimal:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.hex_result_ra_dec = ttk.Label(results_frame, text="—", 
                                          foreground="blue", font=("Courier", 11, "bold"))
        self.hex_result_ra_dec.grid(row=0, column=1, sticky=tk.W, padx=10)
        
        ttk.Label(results_frame, text="RA HMS:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.hex_result_ra_hms = ttk.Label(results_frame, text="—", 
                                          foreground="blue", font=("Courier", 11))
        self.hex_result_ra_hms.grid(row=1, column=1, sticky=tk.W, padx=10)
        
        ttk.Label(results_frame, text="DEC Decimal:").grid(row=2, column=0, sticky=tk.W, pady=5)
        self.hex_result_dec_dec = ttk.Label(results_frame, text="—", 
                                           foreground="blue", font=("Courier", 11, "bold"))
        self.hex_result_dec_dec.grid(row=2, column=1, sticky=tk.W, padx=10)
        
        ttk.Label(results_frame, text="DEC DMS:").grid(row=3, column=0, sticky=tk.W, pady=5)
        self.hex_result_dec_dms = ttk.Label(results_frame, text="—", 
                                           foreground="blue", font=("Courier", 11))
        self.hex_result_dec_dms.grid(row=3, column=1, sticky=tk.W, padx=10)
    
    def create_deg_to_hex_tab(self, notebook):
        """Create Degrees → Hex tab"""
        
        frame = ttk.Frame(notebook, padding="15")
        notebook.add(frame, text="Degrees → Hex")
        
        # RA Section
        ra_frame = ttk.LabelFrame(frame, text="Right Ascension (RA)", padding="10")
        ra_frame.pack(fill=tk.X, pady=(0, 15))
        
        ttk.Label(ra_frame, text="Degrees:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.deg_ra = ttk.Entry(ra_frame, width=20)
        self.deg_ra.grid(row=0, column=1, sticky=tk.W, padx=5)
        ttk.Label(ra_frame, text="(e.g., 180.5)").grid(row=0, column=2, sticky=tk.W, padx=5)
        
        # DEC Section
        dec_frame = ttk.LabelFrame(frame, text="Declination (DEC)", padding="10")
        dec_frame.pack(fill=tk.X, pady=(0, 15))
        
        ttk.Label(dec_frame, text="Degrees:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.deg_dec = ttk.Entry(dec_frame, width=20)
        self.deg_dec.grid(row=0, column=1, sticky=tk.W, padx=5)
        ttk.Label(dec_frame, text="(e.g., -5.25)").grid(row=0, column=2, sticky=tk.W, padx=5)
        
        # Convert button
        btn_frame = ttk.Frame(frame)
        btn_frame.pack(fill=tk.X, pady=15)
        
        ttk.Button(btn_frame, text="Convert", command=self.convert_deg_to_hex,
                  width=20).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Clear", command=self.clear_deg_to_hex,
                  width=20).pack(side=tk.LEFT, padx=5)
        
        # Results frame
        results_frame = ttk.LabelFrame(frame, text="Modbus Registers", padding="10")
        results_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(results_frame, text="RA HI Register:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.deg_result_ra_hi = ttk.Label(results_frame, text="—", 
                                         foreground="blue", font=("Courier", 11, "bold"))
        self.deg_result_ra_hi.grid(row=0, column=1, sticky=tk.W, padx=10)
        
        ttk.Label(results_frame, text="RA LO Register:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.deg_result_ra_lo = ttk.Label(results_frame, text="—", 
                                         foreground="blue", font=("Courier", 11, "bold"))
        self.deg_result_ra_lo.grid(row=1, column=1, sticky=tk.W, padx=10)
        
        ttk.Label(results_frame, text="DEC HI Register:").grid(row=2, column=0, sticky=tk.W, pady=5)
        self.deg_result_dec_hi = ttk.Label(results_frame, text="—", 
                                          foreground="blue", font=("Courier", 11, "bold"))
        self.deg_result_dec_hi.grid(row=2, column=1, sticky=tk.W, padx=10)
        
        ttk.Label(results_frame, text="DEC LO Register:").grid(row=3, column=0, sticky=tk.W, pady=5)
        self.deg_result_dec_lo = ttk.Label(results_frame, text="—", 
                                          foreground="blue", font=("Courier", 11, "bold"))
        self.deg_result_dec_lo.grid(row=3, column=1, sticky=tk.W, padx=10)
    
    # Conversion functions
    
    def hex_to_arcsec100(self, hi_hex: str, lo_hex: str) -> int:
        """Convert HI/LO hex to arcsec*100"""
        hi = int(hi_hex, 16)
        lo = int(lo_hex, 16)
        value32 = (hi << 16) | lo
        
        if value32 >= 0x80000000:
            value32 = value32 - 0x100000000
        
        return value32
    
    def arcsec100_to_degrees(self, arcsec100: int) -> float:
        """Convert arcsec*100 to degrees"""
        return (arcsec100 / 100.0) / 3600.0
    
    def degrees_to_arcsec100(self, degrees: float) -> int:
        """Convert degrees to arcsec*100"""
        return int(degrees * 3600.0 * 100)
    
    def arcsec100_to_hex(self, arcsec100: int) -> tuple:
        """Convert arcsec*100 to HI/LO hex"""
        if arcsec100 < 0:
            value32 = (1 << 32) + arcsec100
        else:
            value32 = arcsec100
        
        hi = (value32 >> 16) & 0xFFFF
        lo = value32 & 0xFFFF
        
        return f"{hi:04X}", f"{lo:04X}"
    
    def format_hms(self, degrees: float) -> str:
        """Format degrees as HH:MM:SS"""
        is_negative = degrees < 0
        degrees = abs(degrees)
        
        hours = int(degrees)
        remainder = (degrees - hours) * 60
        minutes = int(remainder)
        seconds = (remainder - minutes) * 60
        
        sign = "-" if is_negative else ""
        return f"{sign}{hours:02d}:{minutes:02d}:{seconds:06.3f}"
    
    def convert_hex_to_deg(self):
        """Convert Hex → Degrees"""
        try:
            # Validate inputs
            ra_hi = self.hex_ra_hi.get().strip().upper()
            ra_lo = self.hex_ra_lo.get().strip().upper()
            dec_hi = self.hex_dec_hi.get().strip().upper()
            dec_lo = self.hex_dec_lo.get().strip().upper()
            
            #if not all([ra_hi, ra_lo, dec_hi, dec_lo]):
            #    self.show_error("Please fill all hex fields")
            #    return

            ra_hi = ra_hi or "0"
            ra_lo = ra_lo or "0"
            dec_hi = dec_hi or "0"
            dec_lo = dec_lo or "0"
            
            # Convert RA
            ra_arcsec100 = self.hex_to_arcsec100(ra_hi, ra_lo)
            ra_degrees = self.arcsec100_to_degrees(ra_arcsec100)
            ra_hms = self.format_hms(ra_degrees)
            
            # Convert DEC
            dec_arcsec100 = self.hex_to_arcsec100(dec_hi, dec_lo)
            dec_degrees = self.arcsec100_to_degrees(dec_arcsec100)
            dec_dms = self.format_hms(dec_degrees)
            
            # Update results
            self.hex_result_ra_dec.config(text=f"{ra_degrees:.6f}°")
            self.hex_result_ra_hms.config(text=ra_hms)
            self.hex_result_dec_dec.config(text=f"{dec_degrees:.6f}°")
            self.hex_result_dec_dms.config(text=dec_dms)
            
        except ValueError as e:
            self.show_error(f"Invalid hex value: {e}")
    
    def clear_hex_to_deg(self):
        """Clear Hex → Degrees tab"""
        self.hex_ra_hi.delete(0, tk.END)
        self.hex_ra_lo.delete(0, tk.END)
        self.hex_dec_hi.delete(0, tk.END)
        self.hex_dec_lo.delete(0, tk.END)
        
        self.hex_result_ra_dec.config(text="—")
        self.hex_result_ra_hms.config(text="—")
        self.hex_result_dec_dec.config(text="—")
        self.hex_result_dec_dms.config(text="—")
    
    def convert_deg_to_hex(self):
        """Convert Degrees → Hex"""
        try:
            # Get inputs
            ra_str = self.deg_ra.get().strip()
            dec_str = self.deg_dec.get().strip()
            ra_degrees = float(ra_str) if ra_str else 0.0
            dec_degrees = float(dec_str) if dec_str else 0.0
            
            #if not ra_str or not dec_str:
            #    self.show_error("Please enter both RA and DEC degrees")
            #    return
            
            #ra_degrees = float(ra_str)
            #dec_degrees = float(dec_str)

            
            # Convert RA
            ra_arcsec100 = self.degrees_to_arcsec100(ra_degrees)
            ra_hi, ra_lo = self.arcsec100_to_hex(ra_arcsec100)
            
            # Convert DEC
            dec_arcsec100 = self.degrees_to_arcsec100(dec_degrees)
            dec_hi, dec_lo = self.arcsec100_to_hex(dec_arcsec100)
            
            # Update results
            self.deg_result_ra_hi.config(text=ra_hi)
            self.deg_result_ra_lo.config(text=ra_lo)
            self.deg_result_dec_hi.config(text=dec_hi)
            self.deg_result_dec_lo.config(text=dec_lo)
            
        except ValueError:
            self.show_error("Please enter valid decimal numbers")
    
    def clear_deg_to_hex(self):
        """Clear Degrees → Hex tab"""
        self.deg_ra.delete(0, tk.END)
        self.deg_dec.delete(0, tk.END)
        
        self.deg_result_ra_hi.config(text="—")
        self.deg_result_ra_lo.config(text="—")
        self.deg_result_dec_hi.config(text="—")
        self.deg_result_dec_lo.config(text="—")
    
    def show_error(self, message: str):
        """Show error dialog"""
        error_window = tk.Toplevel(self.root)
        error_window.title("Error")
        error_window.geometry("300x100")
        error_window.resizable(False, False)
        
        ttk.Label(error_window, text=message, wraplength=280).pack(pady=20)
        ttk.Button(error_window, text="OK", command=error_window.destroy).pack(pady=10)


def main():
    root = tk.Tk()
    app = HexDegreesConverter(root)
    root.mainloop()


if __name__ == "__main__":
    main()