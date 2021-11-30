#!/usr/bin/env python3
from os import path, rename, environ
try:
    import textract
    from magic import Magic
    use_text_converter = True

    magic = Magic(mime=True)
except ModuleNotFoundError:
    use_text_converter = False


def text_convert(filepath: str) -> bool:
    if not use_text_converter:
        # passthrough
        return True

    mimetype = magic.from_file(filepath)

    # always add an .txt-extension if it is missing
    head, ext = path.splitext(filepath)
    if ext.lower() != '.txt':
        rename(filepath, filepath + '.txt')
        try:
            dirname = path.dirname(filepath)
            filename = path.basename(filepath)
            rename(
                path.join(
                    dirname,
                    f".{filename}.METADATA"),
                path.join(
                    dirname,
                    f".{filename}.txt.METADATA"))
        except FileNotFoundError:
            pass
        filepath += '.txt'

    head, ext = path.splitext(filepath[:-4])

    if mimetype == "text/plain":
        # text is already plaintext, we're done!
        return True

    try:
        if ext.lower() == '.doc':
            # settings for Antiword
            old_environ = dict(environ)
            try:
                environ['ANTIWORDHOME'] = '/usr/share/antiword'
                environ['LC_ALL'] = 'nl_NL@euro IS-8859-15'
                text = textract.process(
                    filepath, extension=ext, encoding='utf_8')
            finally:
                environ.clear()
                environ.update(old_environ)
        else:
            text = textract.process(filepath, extension=ext, encoding='utf_8')
        success = True
    except Exception as error:
        text = f"Unexpected {type(error)}: {error}"
        success = False

    # overwrite the source file, CLAM assumes the source and target
    # location are the same
    with open(filepath, 'wb') as target:
        target.write(text if type(text) == bytes else text.encode('utf-8'))

    return success
