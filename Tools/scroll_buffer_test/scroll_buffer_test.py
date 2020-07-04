import math
import random

from PIL import Image

DEBUG = 0


def _print(*args, **kwargs):
    if DEBUG:
        print(*args, **kwargs)


def img_to_array(img):
    pixels = img.load()
    width, height = img.size
    output = []
    for x in range(width):
        for yByte in range(0, height, 8):
            byte = 0x00
            for y_bit in range(8):
                y = yByte + y_bit
                state = pixels[x, y] > 64
                if state:
                    byte |= (1 << y_bit)
            output.append(byte)
    return output, width, height


def render_frame(dispX, dispY, dispW, dispH, scrollOffsetX, scrollOffsetY):
    dw = 480
    dh = 128
    dhBytes = math.ceil(dh / 8)
    disp_img = Image.new('L', (dw, dh), 255)
    disp_px = disp_img.load()
    disp = img_to_array(disp_img)[0]
    
    sbuf_img = Image.open("sbuf.png").convert('L')
    sbuf, intW, intH = img_to_array(sbuf_img)
    intHBytes = math.ceil(intH / 8)
    
    for x in range(dw):
        if x < dispX or x > dispX + dispW - 1:
            # X range check
            continue
        dispBaseIdx = x * dhBytes
        sbufBaseIdx = ((x - dispX + scrollOffsetX) % intW) * intHBytes
        _print("sbufBaseIdx =", sbufBaseIdx)
        renderByteIdx = 0
        byteOffset = 0
        bitOffset = 0
        for yByte in range(0, dhBytes):
            _print("=" * 25)
            y1 = yByte * 8
            y2 = y1 + 7
            if not (y1 <= dispY + dispH - 1 and dispY <= y2):
                # Y range check: Does the display window intersect the current byte?
                continue
            
            if renderByteIdx == 0:
                # Calculate offsets on first byte to render
                baseBitOffset = (dispY - (yByte * 8)) % 8
                scrollBitOffset = scrollOffsetY - (dispY - (yByte * 8))
                byteOffset, scrollBitOffset = divmod(scrollBitOffset, 8)
                _print("byteOffset =", byteOffset)
                _print("baseBitOffset =", baseBitOffset)
                _print("scrollBitOffset =", scrollBitOffset)
            
            dispIdx = dispBaseIdx + yByte
            sbufIdx = sbufBaseIdx + (renderByteIdx + byteOffset) % intHBytes
            sbufPrevIdx = sbufBaseIdx + (renderByteIdx + byteOffset - 1) % intHBytes
            sbufNextIdx = sbufBaseIdx + (renderByteIdx + byteOffset + 1) % intHBytes
            numRenderBytes = math.ceil((baseBitOffset + dispH) / 8)
            firstByte = renderByteIdx == 0
            lastByte = renderByteIdx >= numRenderBytes - 1
            
            _print("numRenderBytes =", numRenderBytes)
            _print("renderByteIdx =", renderByteIdx)
            _print("firstByte =", firstByte)
            _print("lastByte =", lastByte)
            
            sbufWinAligned = scrollBitOffset == 8 - baseBitOffset   # Scroll buffer is aligned to window
            winDispAligned = baseBitOffset == 0                 # Window is aligned to display
            sbufDispAligned = scrollBitOffset == 0              # Scroll buffer is aligned to display
            
            _print("sbufWinAligned =", sbufWinAligned)
            _print("winDispAligned =", winDispAligned)
            _print("sbufDispAligned =", sbufDispAligned)
                
            if sbufWinAligned and winDispAligned:
                # Everything is aligned, bytes can just be copied.
                # Last byte might have to be combined with display buffer
                # depending on window height.
                if lastByte:
                    keepBits = (renderByteIdx + 1) * 8 - dispH
                    disp[dispIdx] =   (disp[dispIdx] & (0xFF << (8 - keepBits))) \
                                    | (sbuf[sbufIdx] & (0xFF >> keepBits))
                else:
                    disp[dispIdx] = sbuf[sbufIdx]
            elif sbufWinAligned and not winDispAligned:
                # Scroll buffer is aligned to window
                # but window is not aligned to display.
                # First byte will be combined with display buffer,
                # last byte might be combined depending on window height.
                # Scroll buffer bytes will be combined.
                if firstByte and lastByte:
                    # Only one byte, so first = last
                    keepBits = (renderByteIdx + 1) * 8 - baseBitOffset - dispH
                    _print("keepBits =", keepBits)
                    keepMask = (0xFF >> (8 - baseBitOffset)) | (0xFF << (8 - keepBits))
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | (sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~keepMask
                elif firstByte:
                    # First of multiple bytes
                    keepMask = 0xFF >> (8 - baseBitOffset)
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~keepMask)
                elif lastByte:
                    # Last of multiple bytes
                    keepBits = (renderByteIdx + 1) * 8 - baseBitOffset - dispH
                    _print("keepBits =", keepBits)
                    keepMask = 0xFF << (8 - keepBits)
                    sbufMask = 0xFF >> scrollBitOffset
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    _print("sbufMask =", "{:08b}".format(sbufMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | ((sbuf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~sbufMask & ~keepMask)
                else:
                    # Inbetween byte
                    sbufMask = 0xFF >> scrollBitOffset
                    _print("sbufMask =", "{:08b}".format(sbufMask % 256))
                    disp[dispIdx] =   ((sbuf[sbufIdx] >> scrollBitOffset) & sbufMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~sbufMask)
            elif not sbufWinAligned and winDispAligned:
                # Scroll buffer is not aligned to the window
                # but the window is aligned to the display.
                # Last byte will be combined with display buffer
                # depending on window height.
                # Scroll buffer bytes will be combined even in the first byte.
                if lastByte:
                    keepBits = (renderByteIdx + 1) * 8 - dispH
                    keepMask = 0xFF << (8 - keepBits)
                    sbufMask = 0xFF >> scrollBitOffset
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    _print("sbufMask =", "{:08b}".format(sbufMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | ((sbuf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~sbufMask & ~keepMask)
                else:
                    sbufMask = 0xFF >> scrollBitOffset
                    _print("sbufMask =", "{:08b}".format(sbufMask % 256))
                    disp[dispIdx] =   ((sbuf[sbufIdx] >> scrollBitOffset) & sbufMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~sbufMask)
            elif not sbufWinAligned and not winDispAligned and sbufDispAligned:
                # Scroll buffer is not aligned to the window
                # and the window is not aligned to the display,
                # but the scroll buffer is aligned to the display.
                # First byte will be combined with display buffer,
                # Last byte will be combined with display buffer
                # depending on window height.
                # Scroll buffer bytes will not be combined.
                if firstByte and lastByte:
                    # Only one byte, so first = last
                    keepBits = (renderByteIdx + 1) * 8 - baseBitOffset - dispH
                    _print("keepBits =", keepBits)
                    keepMask = (0xFF >> (8 - baseBitOffset)) | (0xFF << (8 - keepBits))
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | (sbuf[sbufIdx] & ~keepMask)
                elif firstByte:
                    # First of multiple bytes
                    keepMask = 0xFF >> (8 - baseBitOffset)
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | (sbuf[sbufIdx] & ~keepMask)
                elif lastByte:
                    # Last of multiple bytes
                    keepBits = (renderByteIdx + 1) * 8 - baseBitOffset - dispH
                    _print("keepBits =", keepBits)
                    keepMask = 0xFF << (8 - keepBits)
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | (sbuf[sbufIdx] & ~keepMask)
                else:
                    # Inbetween byte
                    disp[dispIdx] = sbuf[sbufIdx]
            elif not sbufWinAligned and not winDispAligned and not sbufDispAligned:
                # Scroll buffer is not aligned to the window
                # and the window is not aligned to the display,
                # and the scroll buffer is not aligned to the display.
                # First byte will be combined with display buffer,
                # Last byte will be combined with display buffer
                # depending on window height.
                # Scroll buffer bytes will be combined.
                if firstByte and lastByte:
                    # Only one byte, so first = last
                    keepBits = (renderByteIdx + 1) * 8 - baseBitOffset - dispH
                    _print("keepBits =", keepBits)
                    keepMask = (0xFF >> (8 - baseBitOffset)) | (0xFF << (8 - keepBits))
                    sbufMask = 0xFF >> scrollBitOffset
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    _print("sbufMask =", "{:08b}".format(sbufMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | ((sbuf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~sbufMask & ~keepMask)
                elif firstByte:
                    # First of multiple bytes
                    keepMask = 0xFF >> (8 - baseBitOffset)
                    sbufMask = 0xFF >> scrollBitOffset
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    _print("sbufMask =", "{:08b}".format(sbufMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | ((sbuf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~sbufMask & ~keepMask)
                elif lastByte:
                    # Last of multiple bytes
                    keepBits = (renderByteIdx + 1) * 8 - baseBitOffset - dispH
                    _print("keepBits =", keepBits)
                    keepMask = 0xFF << (8 - keepBits)
                    sbufMask = 0xFF >> scrollBitOffset
                    _print("keepMask =", "{:08b}".format(keepMask % 256))
                    _print("sbufMask =", "{:08b}".format(sbufMask % 256))
                    disp[dispIdx] =   (disp[dispIdx] & keepMask) \
                                    | ((sbuf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~sbufMask & ~keepMask)
                else:
                    # Inbetween byte
                    sbufMask = 0xFF >> scrollBitOffset
                    _print("sbufMask =", "{:08b}".format(sbufMask % 256))
                    disp[dispIdx] =   ((sbuf[sbufIdx] >> scrollBitOffset) & sbufMask) \
                                    | ((sbuf[sbufNextIdx] << (8 - scrollBitOffset)) & ~sbufMask)
            else:
                _print("#" * 50)
                raise RuntimeError
            
            renderByteIdx += 1
    
    for x in range(dw):
        x_base = x * dhBytes
        for yByte in range(0, dhBytes):
            idx = x_base + yByte
            for y_bit in range(8):
                y = yByte * 8 + y_bit
                disp_px[x, y] = 255 if disp[idx] & (1 << y_bit) else 0
    return disp_img


def main():
    r = random.randint
    
    dispX = 16
    dispY = 16
    dispW = 100
    dispH = 64
    scrollOffsetX = 0
    scrollOffsetY = 0
    
    #render_frame(dispX, dispY, dispW, dispH, scrollOffsetX, scrollOffsetY).show()
    #return
    
    frames = []
    
    for x in range(50):
        frame = render_frame(x//2, x//4, x*3, x, x*5, x*3)
        #frame = render_frame(r(0, 250), r(0, 75), r(5, 200), r(5, 100), r(0, 100), r(0, 100))
        #frame = render_frame(dispX, dispY, dispW, dispH, scrollOffsetX, x)
        frames.append(frame)
        #frame.save("{}.png".format(x))
    
    frames[0].save("out.gif", save_all=True, append_images=frames[1:], duration=250, loop=True)


if __name__ == "__main__":
    main()
