#pragma once

#include "Figma/DocumentInterface.h"
#include "Figma/PlayerInterface.h"

namespace Figma
{
    EResult createPlayerImpl(DocumentInterface * const _document, const PlayerDesc & _desc, PlayerInterface ** const _player);
}
