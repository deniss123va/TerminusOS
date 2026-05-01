<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
</head>
<body>

<h1>🌌 TerminusOS v0.4.0</h1>

<p><strong>TerminusOS</strong> is a hobbyist operating system developed from scratch in C++ and x86 Assembly. The project focuses on learning low-level hardware interaction, kernel architecture, and filesystem implementation.</p>
<img src="images/info.png" alt="TerminusOS Info Screen">

<hr>

<h2>🚀 Key Features (v0.4.0)</h2>

<ul>
    <li><strong>FAT32 Filesystem:</strong> Full support for reading, creating, deleting, and renaming files and directories.</li>
    <img src="images/dir2.png" alt="FAT32 Filesystem Navigation" width="500">
    <li><strong>"Nano" Text Editor:</strong> A built-in console editor supporting arrow-key navigation and direct disk saving.</li>
    <img src="images/nano.png" alt="Nano Text Editor" width="500">
    <li><strong>Interrupt Handling:</strong> Implemented Interrupt Descriptor Table (IDT), handling CPU exceptions (ISR) and hardware interrupts (IRQ).</li>
    <li><strong>Keyboard Driver:</strong> Support for scan codes, Shift, Caps Lock, and functional keys.</li>
    <li><strong>UI Customization:</strong> Theme system (Classic, Matrix, Ocean) loaded via <code>boot.cfg</code>.</li>
    <li><strong>Real-Time Clock (RTC):</strong> CMOS integration to display system time and date in the status bar.</li>
</ul>

<hr>

<h2>📂 Project Structure</h2>

<table border="1">
    <tr>
        <th>Directory</th>
        <th>Description</th>
    </tr>
    <tr>
        <td><code>kernel/</code></td>
        <td>Core logic: screen management, IDT, and <code>kmain</code> entry point.</td>
    </tr>
    <tr>
        <td><code>drivers/</code></td>
        <td>Hardware drivers: Keyboard, ATA (disk), and RTC.</td>
    </tr>
    <tr>
        <td><code>fs/</code></td>
        <td>FAT32 filesystem implementation logic.</td>
    </tr>
    <tr>
        <td><code>shell/</code></td>
        <td>Command-line interface and built-in system utilities.</td>
    </tr>
    <tr>
        <td><code>lib/</code></td>
        <td>Standard library: string manipulation and I/O port operations.</td>
    </tr>
</table>

<hr>

<h2>🎨 Available Themes</h2>
<div class="gallery">
    <div class="gallery-item">
        <img src="images/matrix.png" alt="Matrix Theme">
        <p>Matrix</p>
    </div>
    <div class="gallery-item">
        <img src="images/amber.png" alt="Amber Theme">
        <p>Amber</p>
    </div>
    <div class="gallery-item">
        <img src="images/ocean.png" alt="Ocean Theme">
        <p>Ocean</p>
    </div>
    <div class="gallery-item">
        <img src="images/custom.png" alt="Custom Theme">
        <p>Custom (Red/Black)</p>
    </div>
</div>

<hr>

<h2>🛠 Technical Stack</h2>

<ul>
    <li><strong>Languages:</strong> C++, x86 Assembly.</li>
    <li><strong>Architecture:</strong> x86 (32-bit Protected Mode).</li>
    <li><strong>Toolchain:</strong> GCC Cross-Compiler.</li>
</ul>

<hr>

<h2>⌨️ Shell Commands</h2>
<img src="images/help.png" alt="Available Shell Commands" width="600">

<ul>
    <li><code>help</code> — Show available commands.</li>
    <li><code>info</code> — System and developer information.</li>
    <li><code>ls</code> — List directory contents.</li>
    <img src="images/dir.png" alt="List directory command" width="450">
    <li><code>date</code> — Display current date and time.</li>
    <li><code>nano &lt;filename&gt;</code> — Launch the text editor.</li>
    <li><code>theme &lt;name&gt;</code> — Switch the visual color scheme.</li>
</ul>

<hr>

<h2>⚠️ Error Handling</h2>

<p>The system features a <strong>Kernel Panic</strong> mechanism. If a critical error or unhandled exception occurs (e.g., Division by Zero), the kernel halts the CPU and displays diagnostic information on a red background.</p>

<hr>

<h2>👨‍💻 Author</h2>
<p>YouTube: <a href="https://www.youtube.com/@Zero-Logic-dev">Zero Logic</a></p>
<p>Telegram: <a href="https://t.me/den2010991">@den2010991</a></p>
<p>GitHub: <a href="https://github.com/deniss123va">deniss123va</a></p>

</body>
</html>
