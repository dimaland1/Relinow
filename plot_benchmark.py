import matplotlib.pyplot as plt
import numpy as np
import sys

def main():
    # Real Benchmark Measurements (1000 reliable packets of 241 bytes on ESP32 hardware)
    metrics = {
        'Throughput (KB/s)': {'C': 23.47, 'Rust': 14.73, 'unit': 'KB/s', 'better': 'higher'},
        'Total Duration (s)': {'C': 10.03, 'Rust': 15.98, 'unit': 's', 'better': 'lower'},
        'Average RTT (ms)': {'C': 14.0, 'Rust': 18.0, 'unit': 'ms', 'better': 'lower'}
    }

    fig, axes = plt.subplots(1, 3, figsize=(14, 5.5))
    fig.patch.set_facecolor('#0f172a')  # Slate-900 dark background

    c_color = '#38bdf8'    # Sky blue for C
    rust_color = '#fb923c' # Coral orange for Rust

    for i, (title, data) in enumerate(metrics.items()):
        ax = axes[i]
        ax.set_facecolor('#1e293b') # Slate-800
        
        bars = ax.bar(
            ['C (Native)', 'Rust (esp-idf)'], 
            [data['C'], data['Rust']], 
            color=[c_color, rust_color],
            edgecolor='#ffffff', 
            linewidth=1.2,
            width=0.55
        )
        
        ax.set_title(title, fontsize=13, fontweight='bold', color='#f8fafc', pad=15)
        ax.tick_params(axis='x', colors='#cbd5e1', labelsize=11)
        ax.tick_params(axis='y', colors='#94a3b8', labelsize=10)
        
        max_val = max(data['C'], data['Rust'])
        ax.set_ylim(0, max_val * 1.35)
        ax.grid(axis='y', linestyle='--', alpha=0.25, color='#94a3b8')
        
        for bar in bars:
            h = bar.get_height()
            ax.text(
                bar.get_x() + bar.get_width()/2.0, 
                h + (max_val * 0.04), 
                f"{h:.1f} {data['unit']}", 
                ha='center', 
                va='bottom', 
                color='#f8fafc', 
                fontweight='bold', 
                fontsize=11
            )
            
        for spine in ax.spines.values():
            spine.set_color('#334155')

    plt.suptitle(
        "Relinow Reliable ESP-NOW Benchmark (1000 Packets, 241 Bytes)\nPhysical ESP32-WROOM Hardware Comparison", 
        fontsize=15, 
        fontweight='bold', 
        color='#ffffff', 
        y=1.02
    )

    plt.tight_layout()
    plt.savefig('benchmark_comparison.png', dpi=300, facecolor=fig.get_facecolor(), bbox_inches='tight')
    print("Graph saved to benchmark_comparison.png!")

if __name__ == "__main__":
    main()

